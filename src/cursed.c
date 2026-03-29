#include "../lib/cursedhelpers.h"
#include "../lib/gzip/gzip.h"
#include "../lib/deflate/deflate.h"
#include "../lib/bitmap/bitmap.h"
#include "../lib/bitmap/bitmap_cursed.h"
#include "../lib/cursedlib/image/image.h"
#include "../lib/cursedlib/image/filters/greyscale.h"
#include "../lib/png/png.h"
#include "../lib/png/png_cursed.h"

int compress_text(char* data,size_t input_sz,bitarray* bBuffer){
    int wrote_b = 0,total_written = 0,i=0;
    while(total_written<input_sz){
        wrote_b = deflate(bBuffer,data,input_sz);
        if(wrote_b<0){
            LOG_E("Deflate error\n");
            return 0;
        }
        LOG_V("Deflate Blk [%d]: Processed %d bytes\n",i,wrote_b);
        total_written += wrote_b;
        i++;
    }
    bitarray_flush(bBuffer);
    return bBuffer->used;
}

static int str_ends_with(const char* s, const char* suffix){
    size_t slen = strlen(s);
    size_t suflen = strlen(suffix);
    if(suflen > slen) return 0;
    return strcmp(s + slen - suflen, suffix) == 0;
}

static int is_png(const char* path){ return str_ends_with(path, ".png"); }
static int is_bmp(const char* path){ return str_ends_with(path, ".bmp"); }

static cursed_img* read_any(const char* path, uint8_t* out_depth){
    if(is_png(path)){
        png_img* src = read_png(path);
        cursed_img* img;
        if(!src) return NULL;
        *out_depth = src->bit_depth;
        img = png_to_cursed(src);
        free_png_img(src);
        return img;
    }
    if(is_bmp(path)){
        bitmap* src = read_bitmap(path);
        cursed_img* img;
        if(!src) return NULL;
        *out_depth = 8;
        img = bitmap_to_cursed(src);
        free_bitmap(src);
        return img;
    }
    LOG_E("Unsupported input format: %s\n", path);
    return NULL;
}

static int write_any(cursed_img* img, const char* path, uint8_t depth){
    if(is_png(path)){
        png_img* dst = cursed_to_png(img, depth);
        int ok;
        if(!dst) return 0;
        ok = write_png(dst, path);
        free_png_img(dst);
        return ok;
    }
    if(is_bmp(path)){
        bitmap* dst = cursed_to_bitmap(img);
        int ok;
        if(!dst) return 0;
        ok = write_bitmap(dst, path);
        free_bitmap(dst);
        return ok;
    }
    LOG_E("Unsupported output format: %s\n", path);
    return 0;
}

static void apply_filters(cursed_img* img, int argv, char* argc[]){
    int i;
    for(i = 3; i < argv; i++){
        if(strcmp(argc[i], "-grey") == 0){
            cursed_greyscale(img);
        }
    }
}

int main(int argv, char* argc[]){
    int sz = 0;
    bitarray bBuffer = {0};
    FILE* wf;

    if(argv < 2){
        printf("Usage: %s <input> <output> [filters...]\n", argc[0]);
        printf("       %s -f <input> <output>\n", argc[0]);
        printf("Formats: .png .bmp\n");
        printf("Filters: -grey\n");
        return 1;
    }

    if(argc[1][0] == '-' && argc[1][1] == 'f'){
        if(argv < 4){
            LOG_E("Usage: %s -f <input> <output>\n", argc[0]);
            return 1;
        }
        LOG_I("Writing GZIP file\n");
        if(write_gzip_from_file(argc[2], &bBuffer) > 0){
            bitarray_flush(&bBuffer);
            wf = fopen(argc[3], "wb");
            if(!wf){
                LOG_E("Failed to open output file: %s\n", argc[3]);
                return 1;
            }
            fwrite(bBuffer.data, 1, bBuffer.used, wf);
            fclose(wf);
            LOG_I("GZIP file written successfully to: %s\n", argc[3]);
        }
        if(bBuffer.used) free(bBuffer.data);
        return 0;
    }

    if(argv >= 3 && (is_png(argc[1]) || is_bmp(argc[1]))){
        cursed_img* img;
        uint8_t depth = 8;
        if(argv < 3){
            LOG_E("Usage: %s <input> <output> [filters...]\n", argc[0]);
            return 1;
        }
        img = read_any(argc[1], &depth);
        if(!img) return 1;
        apply_filters(img, argv, argc);
        if(!write_any(img, argc[2], depth)){
            free_cursed_img(img);
            return 1;
        }
        free_cursed_img(img);
        return 0;
    }

    /* Default: text compression */
    while(argc[1][sz++]);
    if(!compress_text(argc[1], sz, &bBuffer)){
        LOG_E("Compression failed\n");
        return 1;
    }
    if(bBuffer.used) free(bBuffer.data);
    LOG_V("Compressed data (%ld bytes):\n", bBuffer.used);
    return 0;
}