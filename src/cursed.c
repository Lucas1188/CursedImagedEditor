#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../lib/cursedhelpers.h"
#include "../lib/bitmap/bitmap.h"
#include "../lib/bitmap/bitmap_cursed.h"
#include "../lib/cursedlib/image/image.h"
#include "../lib/cursedlib/image/filters/greyscale.h"
#include "../lib/png/png.h"
#include "../lib/gzip/gzip.h"

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s <input.bmp> <output> [-grey] [-png] [-gzip]\n"
        "  -grey   apply greyscale filter\n"
        "  -png    encode output as PNG\n"
        "  -gzip   compress output with gzip\n"
        "Flags may be combined. -png -gzip produces a gzip-compressed PNG.\n",
        prog);
}

static int has_flag(int argc, char* argv[], const char* flag) {
    int i;
    for (i = 3; i < argc; i++) {
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
    bitmap*     bmp;
    cursed_img* img;
    bitmap*     out_bmp;
    png_s*      png;
    ihdr_chunk  ihdr;
    int         do_grey, do_png, do_gzip;
    const char* tmp_path;

    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    do_grey = has_flag(argc, argv, "-grey");
    do_png  = has_flag(argc, argv, "-png");
    do_gzip = has_flag(argc, argv, "-gzip");
    tmp_path = do_png ? "/tmp/cursed_intermediate.png" : "/tmp/cursed_intermediate.bmp";

    bmp = read_bitmap(argv[1]);
    if (!bmp) {
        LOG_E("Failed to read bitmap: %s\n", argv[1]);
        return 1;
    }

    img = bitmap_to_cursed(bmp);
    free_bitmap(bmp);
    if (!img) {
        LOG_E("Failed to convert bitmap to internal format\n");
        return 1;
    }

    if (do_grey) {
        cursed_greyscale(img);
    }

    out_bmp = cursed_to_bitmap(img);
    RELEASE_CURSED_IMG(*img);
    free(img);
    if (!out_bmp) {
        LOG_E("Failed to convert image to output bitmap\n");
        return 1;
    }

    if (do_png) {
        ihdr.width              = out_bmp->width;
        ihdr.height             = out_bmp->height;
        ihdr.bit_depth          = 8;
        ihdr.color_type         = CT_RGB_ALPHA;
        ihdr.compression_method = 0;
        ihdr.filter_method      = FILTER_NONE;
        ihdr.interlace_method   = INTL_NONE;

        png = create_png(&ihdr, out_bmp->pixels, 4);
        free_bitmap(out_bmp);
        if (!png) {
            LOG_E("Failed to create PNG\n");
            return 1;
        }

        if (do_gzip) {
            if (write_png(tmp_path, png) != 0) {
                free_png(png);
                return 1;
            }
            free_png(png);
            if (!gzip_file_to(tmp_path, argv[2])) {
                remove(tmp_path);
                return 1;
            }
            remove(tmp_path);
        } else {
            if (write_png(argv[2], png) != 0) {
                free_png(png);
                return 1;
            }
            free_png(png);
        }
    } else if (do_gzip) {
        if (!write_bitmap(out_bmp, tmp_path)) {
            free_bitmap(out_bmp);
            return 1;
        }
        free_bitmap(out_bmp);
        if (!gzip_file_to(tmp_path, argv[2])) {
            remove(tmp_path);
            return 1;
        }
        remove(tmp_path);
    } else {
        if (!write_bitmap(out_bmp, argv[2])) {
            free_bitmap(out_bmp);
            return 1;
        }
        free_bitmap(out_bmp);
    }

    return 0;
}
