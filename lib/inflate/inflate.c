#include "inflate.h"
#include "../deflate/deflate.h"
#include "../adler32/adler32.h"
#include "../cursedhelpers.h"
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
   Huffman tree
   Each node is either an internal node (sym == -1, two children) or a leaf
   (sym >= 0, no children). Nodes are stored in a flat pool; children are
   indices into the pool.  INF_NO_CHILD marks a missing edge.
   INF_MAX_NODES covers the largest tree we ever build:
       literal/length: 288 symbols  ->  at most 2*288-1 = 575 nodes
   580 gives a small safety margin.
   ------------------------------------------------------------------------- */
#define INF_NO_CHILD  (-1)
#define INF_MAX_NODES  580

typedef struct {
    int sym;
    int child[2];
} inf_node;

typedef struct {
    inf_node nodes[INF_MAX_NODES];
    int      n;
    int      root;
} inf_tree;

/* -------------------------------------------------------------------------
   Decompressor state
   ------------------------------------------------------------------------- */
typedef struct {
    const uint8_t* src;
    size_t         src_sz;
    size_t         byte_pos;
    int            bit_pos;   /* next bit within src[byte_pos], 0 = LSB */
    uint8_t*       dst;
    size_t         dst_cap;
    size_t         dst_pos;
} inf_state;

/* -------------------------------------------------------------------------
   Static storage for trees (avoids large stack frames)
   ------------------------------------------------------------------------- */
static int      s_fixed_built = 0;
static int      s_fixed_ll_lens[288]; /* literal/length code lengths */
static int      s_fixed_d_lens[32];  /* distance code lengths */
static inf_tree s_fixed_lt;           /* fixed literal/length tree */
static inf_tree s_fixed_dt;           /* fixed distance tree */
static inf_tree s_cl_tree;            /* code-length tree (dynamic blocks) */
static inf_tree s_ll_tree;            /* literal/length tree (dynamic blocks) */
static inf_tree s_d_tree;             /* distance tree (dynamic blocks) */

/* -------------------------------------------------------------------------
   Bit I/O
   DEFLATE packs bits LSB-first within each byte.  Reading them in that
   order means bit 0 of byte 0 is the first bit consumed, which corresponds
   to the most-significant bit of the first Huffman code in the stream.
   ------------------------------------------------------------------------- */
static int inf_read_bit(inf_state* s)
{
    int bit;
    if(s->byte_pos >= s->src_sz) return -1;
    bit = (s->src[s->byte_pos] >> s->bit_pos) & 1;
    if(++s->bit_pos == 8) { s->bit_pos = 0; s->byte_pos++; }
    return bit;
}

static uint32_t inf_read_bits(inf_state* s, int n)
{
    uint32_t val = 0;
    int i;
    for(i = 0; i < n; i++) {
        int b = inf_read_bit(s);
        if(b < 0) return 0;
        val |= ((uint32_t)b << i); /* LSB-first: first bit read -> bit 0 */
    }
    return val;
}

static void inf_byte_align(inf_state* s)
{
    if(s->bit_pos > 0) { s->bit_pos = 0; s->byte_pos++; }
}

/* -------------------------------------------------------------------------
   Huffman tree construction from an array of code lengths.

   Uses the canonical Huffman algorithm from RFC 1951 section 3.2.2:
     1. Count how many symbols have each code length.
     2. Find the first (smallest) code for each length.
     3. Assign codes in symbol order within each length.
     4. Insert each (symbol, code, length) triple into the binary tree,
        traversing from MSB to LSB of the code value.
   ------------------------------------------------------------------------- */
static int inf_tree_build(inf_tree* t, const int* lengths, int n_syms)
{
    int bl_count[16];
    int next_code[16];
    int codes[288]; /* max n_syms we ever call with is 288 */
    int i, len, code, node, bit;

    for(i = 0; i < 16; i++) { bl_count[i] = 0; next_code[i] = 0; }
    for(i = 0; i < n_syms; i++)
        if(lengths[i] > 0 && lengths[i] <= 15)
            bl_count[lengths[i]]++;

    code = 0;
    bl_count[0] = 0;
    for(len = 1; len <= 15; len++) {
        code = (code + bl_count[len - 1]) << 1;
        next_code[len] = code;
    }

    for(i = 0; i < n_syms; i++) {
        if(lengths[i] > 0)
            codes[i] = next_code[lengths[i]]++;
        else
            codes[i] = -1;
    }

    /* initialise root node */
    t->n         = 1;
    t->root      = 0;
    t->nodes[0].sym      = -1;
    t->nodes[0].child[0] = INF_NO_CHILD;
    t->nodes[0].child[1] = INF_NO_CHILD;

    for(i = 0; i < n_syms; i++) {
        if(lengths[i] == 0) continue;
        node = t->root;
        /* walk MSB-first: bit (len-1) down to bit 0 */
        for(bit = lengths[i] - 1; bit >= 0; bit--) {
            int b = (codes[i] >> bit) & 1;
            if(t->nodes[node].child[b] == INF_NO_CHILD) {
                if(t->n >= INF_MAX_NODES) {
                    LOG_E("inflate: tree node pool exhausted\n");
                    return 0;
                }
                t->nodes[t->n].sym      = -1;
                t->nodes[t->n].child[0] = INF_NO_CHILD;
                t->nodes[t->n].child[1] = INF_NO_CHILD;
                t->nodes[node].child[b] = t->n++;
            }
            node = t->nodes[node].child[b];
        }
        t->nodes[node].sym = i;
    }
    return 1;
}

/* Decode one symbol: read bits one at a time, walk the tree. */
static int inf_decode_sym(inf_state* s, const inf_tree* t)
{
    int node = t->root;
    while(t->nodes[node].sym == -1) {
        int b = inf_read_bit(s);
        if(b < 0) return -1;
        node = t->nodes[node].child[b];
        if(node == INF_NO_CHILD) return -1;
    }
    return t->nodes[node].sym;
}

/* -------------------------------------------------------------------------
   Back-reference copy.
   Overlapping copies (length > distance) are intentional: they act as
   run-length encoding.  Reading from 'from + i' into 'dst_pos + i' works
   because when i >= distance we start reading the bytes we just wrote.
   ------------------------------------------------------------------------- */
static int inf_copy_match(inf_state* s, int length, int distance)
{
    size_t from;
    int i;
    if((size_t)distance > s->dst_pos) {
        LOG_E("inflate: back-ref distance exceeds output so far\n");
        return 0;
    }
    from = s->dst_pos - (size_t)distance;
    for(i = 0; i < length; i++) {
        if(s->dst_pos >= s->dst_cap) {
            LOG_E("inflate: output buffer full during match copy\n");
            return 0;
        }
        s->dst[s->dst_pos++] = s->dst[from + i];
    }
    return 1;
}

/* -------------------------------------------------------------------------
   Decode symbols from a block until EOB (symbol 256).
   Shared by both the fixed and dynamic Huffman paths.
   ------------------------------------------------------------------------- */
static int inf_decode_block(inf_state* s, const inf_tree* lt, const inf_tree* dt)
{
    int sym, lcode, dist_sym, length, ebits, distance;
    while(1) {
        sym = inf_decode_sym(s, lt);
        if(sym < 0) { LOG_E("inflate: bad literal/length symbol\n"); return 0; }

        if(sym < 256) {
            /* literal byte */
            if(s->dst_pos >= s->dst_cap) {
                LOG_E("inflate: output buffer full on literal\n");
                return 0;
            }
            s->dst[s->dst_pos++] = (uint8_t)sym;

        } else if(sym == 256) {
            break; /* end-of-block */

        } else {
            /* length/distance back-reference */
            lcode  = sym - 257;
            length = (int)LENCODE_BASE[lcode]
                   + (int)inf_read_bits(s, LENCODE_EBITS[lcode]);

            dist_sym = inf_decode_sym(s, dt);
            if(dist_sym < 0) { LOG_E("inflate: bad distance symbol\n"); return 0; }
            ebits    = DISTANCE_LENS[dist_sym];
            distance = (int)DISTANCE_BASE[dist_sym]
                     + (int)inf_read_bits(s, ebits);

            if(!inf_copy_match(s, length, distance)) return 0;
        }
    }
    return 1;
}

/* -------------------------------------------------------------------------
   Block type 00: stored (no compression)
   ------------------------------------------------------------------------- */
static int inf_stored(inf_state* s)
{
    uint16_t len, nlen;
    inf_byte_align(s);
    if(s->byte_pos + 4 > s->src_sz) {
        LOG_E("inflate: truncated stored block header\n");
        return 0;
    }
    len  = (uint16_t)(s->src[s->byte_pos])
         | ((uint16_t)s->src[s->byte_pos + 1] << 8);
    nlen = (uint16_t)(s->src[s->byte_pos + 2])
         | ((uint16_t)s->src[s->byte_pos + 3] << 8);
    s->byte_pos += 4;
    if((uint16_t)(len ^ nlen) != 0xFFFFu) {
        LOG_E("inflate: stored block LEN/NLEN check failed\n");
        return 0;
    }
    if(s->byte_pos + len > s->src_sz || s->dst_pos + len > s->dst_cap) {
        LOG_E("inflate: stored block overflows buffer\n");
        return 0;
    }
    memcpy(s->dst + s->dst_pos, s->src + s->byte_pos, len);
    s->dst_pos  += len;
    s->byte_pos += len;
    return 1;
}

/* -------------------------------------------------------------------------
   Block type 01: fixed Huffman codes
   Tables are built once and cached in static storage.
   ------------------------------------------------------------------------- */
static int inf_fixed(inf_state* s)
{
    int i;
    if(!s_fixed_built) {
        for(i =   0; i <= 143; i++) s_fixed_ll_lens[i] = 8;
        for(i = 144; i <= 255; i++) s_fixed_ll_lens[i] = 9;
        for(i = 256; i <= 279; i++) s_fixed_ll_lens[i] = 7;
        for(i = 280; i <= 287; i++) s_fixed_ll_lens[i] = 8;
        for(i =   0; i <   32; i++) s_fixed_d_lens[i]  = 5;
        inf_tree_build(&s_fixed_lt, s_fixed_ll_lens, 288);
        inf_tree_build(&s_fixed_dt, s_fixed_d_lens,   32);
        s_fixed_built = 1;
    }
    return inf_decode_block(s, &s_fixed_lt, &s_fixed_dt);
}

/* -------------------------------------------------------------------------
   Block type 10: dynamic Huffman codes
   Reads HLIT, HDIST, HCLEN; decodes the code-length alphabet; uses it to
   decode the literal/length and distance code lengths; builds both trees.
   ------------------------------------------------------------------------- */
static int inf_dynamic(inf_state* s)
{
    int cl_lengths[19];
    int all_lengths[320]; /* max 286 litlen + 32 dist = 318 */
    int hlit, hdist, hclen, i, sym, repeat, val;

    hlit  = (int)inf_read_bits(s, 5) + 257;
    hdist = (int)inf_read_bits(s, 5) + 1;
    hclen = (int)inf_read_bits(s, 4) + 4;

    for(i = 0; i < 19; i++) cl_lengths[i] = 0;
    for(i = 0; i < hclen; i++)
        cl_lengths[CODELEN_ORDER[i]] = (int)inf_read_bits(s, 3);

    if(!inf_tree_build(&s_cl_tree, cl_lengths, 19)) return 0;

    i = 0;
    while(i < hlit + hdist) {
        sym = inf_decode_sym(s, &s_cl_tree);
        if(sym < 0) { LOG_E("inflate: bad code-length symbol\n"); return 0; }

        if(sym < 16) {
            all_lengths[i++] = sym;

        } else if(sym == 16) {
            /* repeat previous length 3-6 times */
            repeat = (int)inf_read_bits(s, 2) + 3;
            val    = (i > 0) ? all_lengths[i - 1] : 0;
            while(repeat-- > 0 && i < hlit + hdist) all_lengths[i++] = val;

        } else if(sym == 17) {
            /* repeat zero 3-10 times */
            repeat = (int)inf_read_bits(s, 3) + 3;
            while(repeat-- > 0 && i < hlit + hdist) all_lengths[i++] = 0;

        } else if(sym == 18) {
            /* repeat zero 11-138 times */
            repeat = (int)inf_read_bits(s, 7) + 11;
            while(repeat-- > 0 && i < hlit + hdist) all_lengths[i++] = 0;

        } else {
            LOG_E("inflate: unexpected code-length symbol %d\n", sym);
            return 0;
        }
    }

    if(!inf_tree_build(&s_ll_tree, all_lengths,        hlit))  return 0;
    if(!inf_tree_build(&s_d_tree,  all_lengths + hlit, hdist)) return 0;

    return inf_decode_block(s, &s_ll_tree, &s_d_tree);
}

/* -------------------------------------------------------------------------
   Public entry point
   ------------------------------------------------------------------------- */
int inflate_zlib(const uint8_t* src, size_t src_sz,
                 uint8_t* dst, size_t* dst_sz)
{
    inf_state  s;
    uint32_t   adler_stored, adler_comp, as1, as2;
    size_t     i;
    int        bfinal, btype;

    if(src_sz < 6) { LOG_E("inflate: stream too short\n"); return 0; }

    /* --- ZLIB header (2 bytes) --- */
    if((src[0] & 0x0F) != 8) {
        LOG_E("inflate: CM != 8 (not DEFLATE)\n"); return 0;
    }
    if(((uint16_t)src[0] * 256 + src[1]) % 31 != 0) {
        LOG_E("inflate: ZLIB header checksum failed\n"); return 0;
    }
    if(src[1] & 0x20) {
        LOG_E("inflate: preset dictionary not supported\n"); return 0;
    }

    s.src      = src;
    s.src_sz   = src_sz - 4; /* last 4 bytes are the ADLER32 trailer */
    s.byte_pos = 2;
    s.bit_pos  = 0;
    s.dst      = dst;
    s.dst_cap  = *dst_sz;
    s.dst_pos  = 0;

    /* --- DEFLATE blocks --- */
    do {
        bfinal = (int)inf_read_bits(&s, 1);
        btype  = (int)inf_read_bits(&s, 2);
        switch(btype) {
        case 0:
            if(!inf_stored(&s))  return 0;
            break;
        case 1:
            if(!inf_fixed(&s))   return 0;
            break;
        case 2:
            if(!inf_dynamic(&s)) return 0;
            break;
        default:
            LOG_E("inflate: reserved BTYPE=3\n");
            return 0;
        }
    } while(!bfinal);

    /* --- ADLER32 verification (big-endian, after byte-aligning) --- */
    inf_byte_align(&s);
    if(s.byte_pos + 4 > src_sz) {
        LOG_E("inflate: missing ADLER32 trailer\n"); return 0;
    }
    adler_stored = ((uint32_t)src[s.byte_pos]     << 24)
                 | ((uint32_t)src[s.byte_pos + 1] << 16)
                 | ((uint32_t)src[s.byte_pos + 2] <<  8)
                 |  (uint32_t)src[s.byte_pos + 3];

    as1 = 1; as2 = 0;
    for(i = 0; i < s.dst_pos; i++) adler32(dst[i], &as1, &as2);
    adler_comp = (as2 << 16) | as1;

    if(adler_stored != adler_comp) {
        LOG_E("inflate: ADLER32 mismatch (file=%08x computed=%08x)\n",
              adler_stored, adler_comp);
        return 0;
    }

    *dst_sz = s.dst_pos;
    LOG_I("inflate: decompressed %zu -> %zu bytes\n", src_sz, s.dst_pos);
    return 1;
}
