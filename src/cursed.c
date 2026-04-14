#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../lib/cursedhelpers.h"
#include "../lib/bitmap/bitmap.h"
#include "../lib/png/png.h"
#include "../lib/gzip/gzip.h"
#include "../include/cursedtui.h"

#ifdef BUILD_ENGINE

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [input.bmp] [output] [-png] [-gzip]\n"
        "  (Run without arguments to launch the Interactive TUI)\n"
        "  -png    convert output to PNG\n"
        "  -gzip   compress output with gzip\n"
        "Flags may be combined. -png -gzip produces a gzip-compressed PNG.\n",
        prog);
}

static int has_flag(int argc, char* argv[], const char* flag) {
    int i;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0) return 1;
    }
    return 0;
}

static int gzip_file_to(const char* src, const char* dst) {
    bitarray bBuffer;
    FILE* wf;

    bBuffer.data     = NULL;
    bBuffer.size     = 0;
    bBuffer.used     = 0;
    bBuffer.bitbuf   = 0;
    bBuffer.bitcount = 0;

    if (write_gzip_from_file(src, &bBuffer) <= 0) {
        LOG_E("Failed to compress: %s\n", src);
        return 0;
    }
    
    bitarray_flush(&bBuffer);
    wf = fopen(dst, "wb");
    if (!wf) {
        LOG_E("Failed to open output: %s\n", dst);
        free(bBuffer.data);
        return 0;
    }
    
    fwrite(bBuffer.data, 1, bBuffer.used, wf);
    fclose(wf);
    free(bBuffer.data);
    return 1;
}

int main(int argc, char* argv[]) {
    bitmap* bmp;
    png_s* png;
    ihdr_chunk ihdr;
    
    int i, do_png, do_gzip;
    const char* input_file = NULL;
    const char* output_file = NULL;
    const char* target_out_path = NULL;

    /* 1. THE DEFAULT: Interactive Mode */
    if (argc == 1) {
        return interactive_mode();
    }

    /* 2. PARSE ARGUMENTS SAFELY */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (!input_file) input_file = argv[i];
            else if (!output_file) output_file = argv[i];
        }
    }

    if (!input_file || !output_file) {
        print_usage(argv[0]);
        return 1;
    }

    do_png  = has_flag(argc, argv, "-png");
    do_gzip = has_flag(argc, argv, "-gzip");

      /* 5. POST-PROCESS (COMPRESSION) STAGE */
    if (do_gzip) {
        if (!gzip_file_to(target_out_path, output_file)) {
            remove(target_out_path); /* Clean up temp file on fail */
            return 1;
        }
        remove(target_out_path); /* Clean up temp file on success */
    }
    /* 3. INPUT STAGE */
    bmp = read_bitmap(input_file);
    if (!bmp) {
        LOG_E("Failed to read bitmap: %s\n", input_file);
        return 1;
    }

    /* 4. CONVERT / EXPORT STAGE */
    /* If we are going to GZIP, write the raw image to a temp file first! */
    target_out_path = do_gzip ? "temp_export.raw" : output_file;

    if (do_png) {
        ihdr = IHDR_TRUECOLOR8_A8(bmp->width, bmp->height);
        png = create_png(&ihdr, bmp->pixels, 4);
        if (!png || write_png(target_out_path, png) != 0) {
            LOG_E("Failed to convert and write PNG\n");
            if (png) free_png(png);
            free_bitmap(bmp);
            return 1;
        }
        free_png(png);
    } else {
        if (!write_bitmap(bmp, target_out_path)) {
            LOG_E("Failed to write BMP\n");
            free_bitmap(bmp);
            return 1;
        }
    }
    
    free_bitmap(bmp); /* Free original image memory before gzip phase */

  

    return 0;
}
#endif