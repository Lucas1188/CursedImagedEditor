#include "image.h"
#include <stdio.h>

#ifdef STANDALONE_CURSEDIMAGE
int main(int argv, char** argc){
    cursed_img i = GEN_CURSED_IMG(1280,720);
    RELEASE_CURSED_IMG(i);
    return 0;
}
#endif
