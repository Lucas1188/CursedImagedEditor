#ifdef USE_PACKER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bithelper/bithelpers.h"
#include "cursedhelpers.h"
#include "gzip/gzip.h"
#include "base64encoder/b64e.h"

/* ---------------- MOCK LOGGER ---------------- */
void console_log_mock(const char* msg) {
    printf("%s", msg);
}
void (*cursed_log_callback)(const char*) = console_log_mock;

/* ---------------- BIT BUFFER ---------------- */
void bitarray_init(bitarray *b, size_t initial_size) {
    b->data = (uint8_t*)malloc(initial_size);
    b->size = initial_size;
    b->used = 0;
    b->bitbuf = 0;
    b->bitcount = 0;
}

void bitarray_free(bitarray *b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->size = 0;
    b->used = 0;
}

/* ---------------- EXTERNALS ---------------- */
extern uint32_t write_gzip_from_file(const char* filename, bitarray* bData);

extern int _getb64size(long fsize, long* bsize_out, long* remainder_out, long* blocks_out);

extern int b64_encode(
    const unsigned char* in,
    size_t in_size,
    unsigned char* out,
    size_t* out_size
);

/* ---------------- TAR HEADER ---------------- */
typedef struct {
    char name[100]; char mode[8]; char uid[8]; char gid[8];
    char size[12]; char mtime[12]; char chksum[8]; char typeflag;
    char linkname[100]; char magic[6]; char version[2];
    char uname[32]; char gname[32]; char devmajor[8]; char devminor[8];
    char prefix[155]; char pad[12];
} tar_header;

void calc_tar_checksum(tar_header* h) {
    unsigned int sum = 0;
    unsigned char* p = (unsigned char*)h;
    size_t i;

    memset(h->chksum, ' ', 8);

    for (i = 0; i < 512; i++)
        sum += p[i];

    sprintf(h->chksum, "%06o", sum);
    h->chksum[6] = '\0';
    h->chksum[7] = ' ';
}

/* ---------------- MINIFIER ---------------- */
size_t minify_c_file(FILE* in, FILE* out) {
    int c;
    int in_string = 0, in_char = 0, in_macro = 0, in_comment = 0, last_space = 0;
    size_t bytes = 0;

    while ((c = fgetc(in)) != EOF) {

        if (!in_comment && !in_macro) {
            if (c == '"' && !in_char) in_string = !in_string;
            if (c == '\'' && !in_string) in_char = !in_char;
        }

        if (!in_string && !in_char && !in_comment && c == '#')
            in_macro = 1;

        if (in_macro && c == '\n')
            in_macro = 0;

        if (!in_string && !in_char && c == '/') {
            int next = fgetc(in);
            if (next == '*') { in_comment = 1; continue; }
            ungetc(next, in);
        }

        if (in_comment) {
            if (c == '*') {
                int next = fgetc(in);
                if (next == '/') { in_comment = 0; continue; }
                ungetc(next, in);
            }
            continue;
        }

        if (!in_string && !in_char && !in_macro && isspace(c)) {
            if (!last_space) {
                fputc(' ', out);
                bytes++;
                last_space = 1;
            }
            continue;
        } else {
            last_space = 0;
        }

        fputc(c, out);
        bytes++;
    }

    return bytes;
}
/* ---------------- MAIN PACKER ---------------- */
int main(int argc, char** argv) {

    int i;
    FILE *tar_stream, *final_out;
    const char* temp_tar = "temp_payload.tar";

    bitarray gz_data;

    long b64_sz, rem, blocks;
    unsigned char *b64_buf;
    size_t final_b64_sz;
    uint32_t gz_size;

    char zero[512] = {0};

    if (argc < 3) {
        printf("Usage: %s <out.b64> <files...>\n", argv[0]);
        return 1;
    }

    /* ---------------- TAR ---------------- */
    printf("[PACKER] tar...\n");

    tar_stream = fopen(temp_tar, "wb");
    if (!tar_stream) return 1;

    for (i = 2; i < argc; i++) {

        FILE *in = fopen(argv[i], "rb");
        if (!in) continue;

        FILE *tmp = tmpfile();

        size_t sz = minify_c_file(in, tmp);
        fclose(in);

        tar_header h;
        memset(&h, 0, sizeof(h));

        strncpy(h.name, argv[i], 99);
        strcpy(h.mode, "0000644");
        sprintf(h.size, "%011lo", (unsigned long)sz);
        strcpy(h.magic, "ustar");

        h.version[0] = '0';
        h.version[1] = '0';

        calc_tar_checksum(&h);

        fwrite(&h, 1, 512, tar_stream);

        rewind(tmp);
        int c;
        while ((c = fgetc(tmp)) != EOF)
            fputc(c, tar_stream);

        fclose(tmp);

        size_t pad = 512 - (sz % 512);
        if (pad < 512)
            fwrite(zero, 1, pad, tar_stream);
    }

    fwrite(zero, 1, 512, tar_stream);
    fwrite(zero, 1, 512, tar_stream);
    fclose(tar_stream);

    /* ---------------- GZIP ---------------- */
    printf("[PACKER] gzip...\n");

    bitarray_init(&gz_data, 1024 * 1024);

    gz_size = write_gzip_from_file(temp_tar, &gz_data);

    if (gz_size == 0)
        gz_size = gz_data.used;

    if (gz_size == 0 || !gz_data.data) {
        printf("gzip failed\n");
        return 1;
    }

    /* ---------------- BASE64 ---------------- */
    printf("[PACKER] b64...\n");

    _getb64size(gz_size, &b64_sz, &rem, &blocks);

    b64_buf = malloc(b64_sz + 4);

    if (!b64_buf) return 1;

    b64_encode(gz_data.data, gz_size, b64_buf, &final_b64_sz);

    b64_buf[final_b64_sz] = '\0';

    final_out = fopen(argv[1], "w");
    if (final_out) {
        fwrite(b64_buf, 1, final_b64_sz, final_out);
        fclose(final_out);
    }

    free(b64_buf);
    bitarray_free(&gz_data);
    remove(temp_tar);

    printf("[PACKER] done\n");
    return 0;
}
#endif