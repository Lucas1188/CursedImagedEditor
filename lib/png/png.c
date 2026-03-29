#include "png.h"
#include "../inflate/inflate.h"
#include "../crc32/crc32.h"
#include "../cursedhelpers.h"
#include "../bithelper/bithelpers.h"
#include <stdlib.h>
#include <string.h>

/* Forward-declare to avoid pulling in zlib.h (which has file-scope const
   definitions that cause duplicate-symbol errors when included in more than
   one translation unit). */
int write_zlib_from_buf(const uint8_t* data, size_t sz, bitarray* bData);

/* -------------------------------------------------------------------------
   Chunk type identifiers (four ASCII bytes packed big-endian)
   ------------------------------------------------------------------------- */
#define CHUNK_IHDR 0x49484452u
#define CHUNK_PLTE 0x504C5445u
#define CHUNK_IDAT 0x49444154u
#define CHUNK_IEND 0x49454E44u

/* PNG file magic: 8 bytes */
static const uint8_t PNG_MAGIC[8] = {137, 80, 78, 71, 13, 10, 26, 10};

/* -------------------------------------------------------------------------
   Helpers: big-endian I/O
   ------------------------------------------------------------------------- */
static int read_u32_be(FILE* f, uint32_t* out)
{
    uint8_t b[4];
    if(fread(b, 1, 4, f) != 4) return 0;
    *out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
    return 1;
}

static void write_u32_be(FILE* f, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >>  8);
    b[3] = (uint8_t)(v);
    fwrite(b, 1, 4, f);
}

/* -------------------------------------------------------------------------
   Write one PNG chunk: length (4 BE) + type (4 BE) + data + CRC32 (4 BE).
   CRC covers type bytes + data bytes (not the length field).
   ------------------------------------------------------------------------- */
static void write_chunk(FILE* f, uint32_t type,
                        const uint8_t* data, uint32_t dlen)
{
    uint8_t  tbuf[4];
    uint32_t crc;

    write_u32_be(f, dlen);

    tbuf[0] = (uint8_t)(type >> 24);
    tbuf[1] = (uint8_t)(type >> 16);
    tbuf[2] = (uint8_t)(type >>  8);
    tbuf[3] = (uint8_t)(type);
    fwrite(tbuf, 1, 4, f);

    if(data && dlen > 0) fwrite(data, 1, dlen, f);

    crc = crc32(0, tbuf, 4);
    if(data && dlen > 0) crc = crc32(crc, data, dlen);
    write_u32_be(f, crc);
}

/* -------------------------------------------------------------------------
   Number of channels for a given PNG color type
   ------------------------------------------------------------------------- */
static int png_channels(uint8_t color_type)
{
    switch(color_type) {
    case CT_GRAYSCALE:  return 1;
    case CT_RGB:        return 3;
    case CT_PALETE:     return 1;
    case CT_GRAY_ALPHA: return 2;
    case CT_RGB_ALPHA:  return 4;
    default:            return 0;
    }
}

/* -------------------------------------------------------------------------
   Paeth predictor (PNG spec section 9.4)
   ------------------------------------------------------------------------- */
static int paeth(int a, int b, int c)
{
    int p  = a + b - c;
    int pa = p - a; if(pa < 0) pa = -pa;
    int pb = p - b; if(pb < 0) pb = -pb;
    int pc = p - c; if(pc < 0) pc = -pc;
    if(pa <= pb && pa <= pc) return a;
    if(pb <= pc)             return b;
    return c;
}

/* -------------------------------------------------------------------------
   Reverse one PNG filter row.

   row      - pointer to the raw (still-filtered) pixel bytes for this row
   prior    - pointer to the already-reconstructed previous row
               (caller passes a zero-filled buffer for the first row)
   row_len  - number of bytes in the row (NOT counting the filter byte)
   bpp      - filter bytes-per-pixel = ceil(bit_depth/8) * channels
   filter   - the filter type byte that precedes this row in the stream
   ------------------------------------------------------------------------- */
static void unfilter_row(uint8_t filter, uint8_t* row,
                         const uint8_t* prior, int row_len, int bpp)
{
    int x, a, b, c;
    switch(filter) {
    case 0: /* None */
        break;

    case 1: /* Sub: Recon(x) = Filt(x) + Recon(x-bpp) */
        for(x = bpp; x < row_len; x++)
            row[x] = (uint8_t)(row[x] + row[x - bpp]);
        break;

    case 2: /* Up: Recon(x) = Filt(x) + Prior(x) */
        for(x = 0; x < row_len; x++)
            row[x] = (uint8_t)(row[x] + prior[x]);
        break;

    case 3: /* Average: Recon(x) = Filt(x) + floor((Recon(x-bpp)+Prior(x))/2) */
        for(x = 0; x < row_len; x++) {
            a = (x >= bpp) ? row[x - bpp] : 0;
            b = prior[x];
            row[x] = (uint8_t)(row[x] + (unsigned)((a + b) >> 1));
        }
        break;

    case 4: /* Paeth */
        for(x = 0; x < row_len; x++) {
            a = (x >= bpp) ? row[x - bpp]        : 0;
            b = prior[x];
            c = (x >= bpp) ? prior[x - bpp]      : 0;
            row[x] = (uint8_t)(row[x] + (unsigned)paeth(a, b, c));
        }
        break;

    default:
        LOG_E("png: unknown filter type %u\n", (unsigned)filter);
        break;
    }
}

/* -------------------------------------------------------------------------
   Convert one unfiltered raw row (native PNG format) to RGBA and write it
   into the output pixel buffer at dst_row.

   dst_row  - output: 4 bytes/px (8-bit) or 8 bytes/px (16-bit), RGBA
   src      - input: raw row bytes after filter reconstruction
   ct       - color_type
   bd       - bit_depth (8 or 16)
   width    - image width in pixels
   ------------------------------------------------------------------------- */
static void row_to_rgba(uint8_t* dst_row, const uint8_t* src,
                        uint8_t ct, uint8_t bd, uint32_t width)
{
    uint32_t x;
    uint32_t bps = (uint32_t)bd / 8u; /* bytes per sample: 1 or 2 */
    uint32_t out_stride = 4u * bps;   /* bytes per RGBA pixel in output */

    for(x = 0; x < width; x++) {
        uint8_t* d = dst_row + x * out_stride;

        if(bd == 16) {
            /* All 16-bit paths — values are big-endian pairs */
            const uint8_t* s = src + x * (uint32_t)png_channels(ct) * 2u;
            switch(ct) {
            case CT_GRAYSCALE: /* [Gh Gl] -> RGBA16 */
                d[0]=s[0]; d[1]=s[1]; /* R = gray */
                d[2]=s[0]; d[3]=s[1]; /* G = gray */
                d[4]=s[0]; d[5]=s[1]; /* B = gray */
                d[6]=0xFF; d[7]=0xFF; /* A = opaque */
                break;
            case CT_GRAY_ALPHA: /* [Gh Gl Ah Al] -> RGBA16 */
                d[0]=s[0]; d[1]=s[1];
                d[2]=s[0]; d[3]=s[1];
                d[4]=s[0]; d[5]=s[1];
                d[6]=s[2]; d[7]=s[3];
                break;
            case CT_RGB: /* [Rh Rl Gh Gl Bh Bl] -> RGBA16 */
                d[0]=s[0]; d[1]=s[1];
                d[2]=s[2]; d[3]=s[3];
                d[4]=s[4]; d[5]=s[5];
                d[6]=0xFF; d[7]=0xFF;
                break;
            case CT_RGB_ALPHA: /* already RGBA16 */
                d[0]=s[0]; d[1]=s[1];
                d[2]=s[2]; d[3]=s[3];
                d[4]=s[4]; d[5]=s[5];
                d[6]=s[6]; d[7]=s[7];
                break;
            default:
                break;
            }
        } else {
            /* All 8-bit paths */
            const uint8_t* s = src + x * (uint32_t)png_channels(ct);
            switch(ct) {
            case CT_GRAYSCALE: /* [g] -> RGBA8 */
                d[0]=s[0]; d[1]=s[0]; d[2]=s[0]; d[3]=0xFF;
                break;
            case CT_GRAY_ALPHA: /* [g a] -> RGBA8 */
                d[0]=s[0]; d[1]=s[0]; d[2]=s[0]; d[3]=s[1];
                break;
            case CT_RGB: /* [r g b] -> RGBA8 */
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=0xFF;
                break;
            case CT_RGB_ALPHA: /* already RGBA8 */
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=s[3];
                break;
            default:
                break;
            }
        }
    }
}

/* =========================================================================
   read_png
   ========================================================================= */
png_img* read_png(const char* filename)
{
    FILE*    f;
    uint8_t  magic[8];
    uint32_t chunk_len, chunk_type, crc_file, crc_comp;
    uint8_t  tbuf[4];
    uint8_t* chunk_data;

    /* IHDR fields */
    uint32_t width = 0, height = 0;
    uint8_t  bit_depth = 0, color_type = 0, compression = 0;
    uint8_t  filter_meth = 0, interlace = 0;
    int      ihdr_seen = 0;

    /* IDAT accumulator */
    uint8_t* idat_buf  = NULL;
    size_t   idat_sz   = 0;
    size_t   idat_cap  = 0;

    /* Decompressed pixel data */
    uint8_t* raw       = NULL;
    size_t   inflate_cap, inflated_sz;
    int      n_ch, bps, row_bytes, filt_bpp;

    /* Output */
    png_img* img       = NULL;
    uint32_t r;
    uint8_t* zeros     = NULL;
    size_t   pix_stride;

    f = fopen(filename, "rb");
    if(!f) { LOG_E("read_png: cannot open %s\n", filename); return NULL; }

    /* --- Magic --- */
    if(fread(magic, 1, 8, f) != 8 || memcmp(magic, PNG_MAGIC, 8) != 0) {
        LOG_E("read_png: not a PNG file: %s\n", filename);
        fclose(f); return NULL;
    }

    /* --- Chunk loop --- */
    for(;;) {
        if(!read_u32_be(f, &chunk_len) || !read_u32_be(f, &chunk_type)) {
            LOG_E("read_png: truncated chunk header\n");
            goto fail;
        }

        /* Read chunk data */
        chunk_data = NULL;
        if(chunk_len > 0) {
            chunk_data = (uint8_t*)malloc(chunk_len);
            if(!chunk_data) { LOG_E("read_png: OOM chunk\n"); goto fail; }
            if(fread(chunk_data, 1, chunk_len, f) != chunk_len) {
                LOG_E("read_png: truncated chunk data\n");
                free(chunk_data); goto fail;
            }
        }

        /* Read and verify CRC */
        if(!read_u32_be(f, &crc_file)) {
            free(chunk_data); goto fail;
        }
        tbuf[0] = (uint8_t)(chunk_type >> 24);
        tbuf[1] = (uint8_t)(chunk_type >> 16);
        tbuf[2] = (uint8_t)(chunk_type >>  8);
        tbuf[3] = (uint8_t)(chunk_type);
        crc_comp = crc32(0, tbuf, 4);
        if(chunk_data && chunk_len > 0)
            crc_comp = crc32(crc_comp, chunk_data, chunk_len);
        if(crc_file != crc_comp)
            LOG_E("read_png: CRC mismatch in chunk %08x (ignored)\n", chunk_type);

        /* --- Dispatch on chunk type --- */
        if(chunk_type == CHUNK_IHDR) {
            if(chunk_len < 13) { free(chunk_data); goto fail; }
            width       = ((uint32_t)chunk_data[0]<<24)|((uint32_t)chunk_data[1]<<16)
                        | ((uint32_t)chunk_data[2]<< 8)| (uint32_t)chunk_data[3];
            height      = ((uint32_t)chunk_data[4]<<24)|((uint32_t)chunk_data[5]<<16)
                        | ((uint32_t)chunk_data[6]<< 8)| (uint32_t)chunk_data[7];
            bit_depth   = chunk_data[8];
            color_type  = chunk_data[9];
            compression = chunk_data[10];
            filter_meth = chunk_data[11];
            interlace   = chunk_data[12];

            if(bit_depth != 8 && bit_depth != 16) {
                LOG_E("read_png: unsupported bit depth %u\n", bit_depth);
                free(chunk_data); goto fail;
            }
            if(color_type == CT_PALETE) {
                LOG_E("read_png: indexed-color PNG not supported\n");
                free(chunk_data); goto fail;
            }
            if(png_channels(color_type) == 0) {
                LOG_E("read_png: unknown color type %u\n", color_type);
                free(chunk_data); goto fail;
            }
            if(compression != 0) {
                LOG_E("read_png: unknown compression method %u\n", compression);
                free(chunk_data); goto fail;
            }
            if(filter_meth != 0) {
                LOG_E("read_png: unknown filter method %u\n", filter_meth);
                free(chunk_data); goto fail;
            }
            if(interlace != 0) {
                LOG_E("read_png: interlaced PNG not supported\n");
                free(chunk_data); goto fail;
            }
            ihdr_seen = 1;
            LOG_I("read_png: %ux%u bd=%u ct=%u\n",
                  width, height, bit_depth, color_type);
            free(chunk_data);

        } else if(chunk_type == CHUNK_IDAT) {
            /* Accumulate compressed data from all IDAT chunks */
            if(idat_sz + chunk_len > idat_cap) {
                idat_cap = (idat_sz + chunk_len) * 2 + 4096;
                idat_buf = (uint8_t*)realloc(idat_buf, idat_cap);
                if(!idat_buf) { LOG_E("read_png: OOM idat\n"); free(chunk_data); goto fail; }
            }
            if(chunk_data && chunk_len > 0) {
                memcpy(idat_buf + idat_sz, chunk_data, chunk_len);
                idat_sz += chunk_len;
            }
            free(chunk_data);

        } else if(chunk_type == CHUNK_IEND) {
            free(chunk_data);
            break;

        } else {
            /* Unknown/ancillary chunk: skip */
            free(chunk_data);
        }
    }
    fclose(f);
    f = NULL;

    if(!ihdr_seen || idat_sz == 0) {
        LOG_E("read_png: missing IHDR or IDAT\n"); goto fail;
    }

    /* --- Inflate --- */
    n_ch        = png_channels(color_type);
    bps         = (int)bit_depth / 8;
    row_bytes   = (int)width * n_ch * bps;
    filt_bpp    = n_ch * bps;
    inflate_cap = (size_t)height * (size_t)(1 + row_bytes);

    raw = (uint8_t*)malloc(inflate_cap);
    if(!raw) { LOG_E("read_png: OOM raw\n"); goto fail; }

    inflated_sz = inflate_cap;
    if(!inflate_zlib(idat_buf, idat_sz, raw, &inflated_sz)) {
        LOG_E("read_png: inflate failed\n"); goto fail;
    }
    free(idat_buf);
    idat_buf = NULL;

    if(inflated_sz != inflate_cap) {
        LOG_E("read_png: unexpected decompressed size\n"); goto fail;
    }

    /* --- Allocate output image --- */
    img = (png_img*)malloc(sizeof(png_img));
    if(!img) goto fail;
    img->width      = width;
    img->height     = height;
    img->bit_depth  = bit_depth;
    img->color_type = color_type;
    pix_stride      = 4u * (size_t)bps; /* bytes per RGBA pixel */
    img->pixels = (uint8_t*)malloc(width * height * pix_stride);
    if(!img->pixels) { free(img); img = NULL; goto fail; }

    /* --- Un-filter and convert rows --- */
    zeros = (uint8_t*)calloc(1, (size_t)row_bytes);
    if(!zeros) goto fail;

    for(r = 0; r < height; r++) {
        uint8_t* row_raw   = raw + (size_t)r * (size_t)(1 + row_bytes) + 1;
        uint8_t  filt_byte = raw[(size_t)r * (size_t)(1 + row_bytes)];
        uint8_t* prior;

        if(r == 0) {
            prior = zeros;
        } else {
            prior = raw + (size_t)(r - 1) * (size_t)(1 + row_bytes) + 1;
        }

        unfilter_row(filt_byte, row_raw, prior, row_bytes, filt_bpp);
        row_to_rgba(img->pixels + (size_t)r * (size_t)width * pix_stride,
                    row_raw, color_type, bit_depth, width);
    }

    free(zeros);
    free(raw);
    LOG_I("read_png: loaded %ux%u %u-bit\n", width, height, bit_depth);
    return img;

fail:
    if(f)        fclose(f);
    if(idat_buf) free(idat_buf);
    if(raw)      free(raw);
    if(zeros)    free(zeros);
    if(img) { free(img->pixels); free(img); }
    return NULL;
}

/* =========================================================================
   write_png
   Outputs a CT_RGB_ALPHA PNG at the bit depth recorded in img.
   Uses filter type 0 (None) for every row — simple and lossless.
   ========================================================================= */
int write_png(const png_img* img, const char* filename)
{
    FILE*    f;
    uint8_t  ihdr_data[13];
    uint8_t* filt_rows;    /* filter-byte-prefixed uncompressed pixel data */
    bitarray bData;
    uint32_t row_bytes;    /* bytes per row in the output (no filter byte) */
    size_t   filt_sz;
    uint32_t r, x;
    uint8_t* src_pix;
    uint8_t* dst_row;
    int      bps;          /* bytes per sample: 1 or 8-bit, 2 for 16-bit */

    if(!img || !img->pixels) return 0;
    if(img->bit_depth != 8 && img->bit_depth != 16) return 0;

    bps       = (int)img->bit_depth / 8;
    row_bytes = img->width * 4u * (uint32_t)bps; /* RGBA = 4 channels */
    filt_sz   = (size_t)img->height * (size_t)(1 + row_bytes);

    filt_rows = (uint8_t*)malloc(filt_sz);
    if(!filt_rows) { LOG_E("write_png: OOM filter rows\n"); return 0; }

    /* Build filter-byte-prefixed rows (filter 0 = None) */
    for(r = 0; r < img->height; r++) {
        dst_row  = filt_rows + (size_t)r * (size_t)(1 + row_bytes);
        src_pix  = img->pixels + (size_t)r * (size_t)(img->width) * 4u * (size_t)bps;
        dst_row[0] = 0; /* filter type None */

        if(bps == 2) {
            /* 16-bit: pixels are already big-endian RGBA pairs */
            memcpy(dst_row + 1, src_pix, row_bytes);
        } else {
            /* 8-bit: copy RGBA bytes directly */
            for(x = 0; x < img->width * 4u; x++)
                dst_row[1 + x] = src_pix[x];
        }
    }

    /* Compress with ZLIB */
    memset(&bData, 0, sizeof(bitarray));
    if(write_zlib_from_buf(filt_rows, filt_sz, &bData) < 0) {
        LOG_E("write_png: compression failed\n");
        free(filt_rows);
        return 0;
    }
    free(filt_rows);

    f = fopen(filename, "wb");
    if(!f) {
        LOG_E("write_png: cannot open %s\n", filename);
        free(bData.data);
        return 0;
    }

    /* Magic */
    fwrite(PNG_MAGIC, 1, 8, f);

    /* IHDR chunk (13 bytes of data) */
    ihdr_data[0]  = (uint8_t)(img->width  >> 24);
    ihdr_data[1]  = (uint8_t)(img->width  >> 16);
    ihdr_data[2]  = (uint8_t)(img->width  >>  8);
    ihdr_data[3]  = (uint8_t)(img->width);
    ihdr_data[4]  = (uint8_t)(img->height >> 24);
    ihdr_data[5]  = (uint8_t)(img->height >> 16);
    ihdr_data[6]  = (uint8_t)(img->height >>  8);
    ihdr_data[7]  = (uint8_t)(img->height);
    ihdr_data[8]  = img->bit_depth;
    ihdr_data[9]  = CT_RGB_ALPHA;
    ihdr_data[10] = 0; /* compression method 0 */
    ihdr_data[11] = 0; /* filter method 0 */
    ihdr_data[12] = 0; /* no interlace */
    write_chunk(f, CHUNK_IHDR, ihdr_data, 13);

    /* IDAT chunk */
    write_chunk(f, CHUNK_IDAT, bData.data, (uint32_t)bData.used);
    free(bData.data);

    /* IEND chunk (no data) */
    write_chunk(f, CHUNK_IEND, NULL, 0);

    fclose(f);
    LOG_I("write_png: wrote %ux%u %u-bit to %s\n",
          img->width, img->height, img->bit_depth, filename);
    return 1;
}

/* =========================================================================
   free_png_img
   ========================================================================= */
void free_png_img(png_img* img)
{
    if(!img) return;
    free(img->pixels);
    free(img);
}
