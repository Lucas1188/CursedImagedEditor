#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "bithelper/bithelpers.h"
#include "gzip/gzip.h"
#include "base64encoder/b64e.h"
#include "cursedhelpers.h"
#include "base64encoder/b64tbl.h"

#ifdef BUILD_PACKER

/* * -----------------------------------------------------------------
 * BASE64 IMPLEMENTATION (MATCHING YOUR TEST DRIVER)
 * -----------------------------------------------------------------
 */

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
    bitarray bData = {0};
    FILE *tar_f, *fTpl;
    const char* temp_tar = "temp_payload.tar";
    char *tpl_buf, *marker_pos, *final_html;
    unsigned char *b64_payload, *b64_final;
    size_t b64_payload_sz = 0, b64_final_sz = 0;
    long tpl_sz;
    int i;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <template.html> <binaries...>\n", argv[0]);
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
    long p_bsize, p_rem, p_blocks;
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
    long f_bsize, f_rem, f_blocks;
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