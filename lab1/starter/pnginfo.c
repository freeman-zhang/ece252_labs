#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                */
#include <errno.h>              /* for errno                   */
#include <string.h>             /* for strcpy                  */
#include "png_util/crc.h"       /* for crc()                   */
#include "png_util/lab_png.h"   /* simple PNG data structures  */

int main(int argc, char *argv[]){
    /* Get path for png */
    if (argc == 1) {
        printf("Usage: %s png_file\n", argv[0]);
        return -1;
    }

    simple_PNG_p png = createPNG(argv[1]);
    if(png->errCode == -1){
        printf("Could not find PNG\n");
        return -1;
    }
    
    U8 pngPath[255];
    strcpy(pngPath, argv[1]);
    U8* buffer = malloc(8);
    FILE *pngCheck = fopen(pngPath, "rb");
    if(pngCheck == NULL){
        return -1;
    }
    memset(buffer, 0, 8);
    fread(buffer, 8, 1, pngCheck);
    
    /* Get file name as path */
    char *pngName = pngPath;
    char *ssc;
    int l = 0;
    ssc = strstr(pngName, "/");
    if(ssc){
        do{
            l = strlen(ssc) + 1;
            pngName = &pngName[strlen(pngName)-l+2];
            ssc = strstr(pngName, "/");
        }while(ssc);
    }
    else{
        pngName = pngPath;
    }

    if(png->errCode == -2){
        printf("%s: NOT a PNG file\n", pngName);
        return -1;
    }
    free(buffer);

    data_IHDR_p ihdr_data = malloc(sizeof(data_IHDR_p));

    int width, height = 0;

    width = get_png_width(png->p_IHDR);
    height = get_png_height(png->p_IHDR);

    ihdr_data->width = width;
    ihdr_data->height = height;
    ihdr_data->bit_depth = png->p_IHDR->p_data[8];
    ihdr_data->color_type = png->p_IHDR->p_data[9];
    ihdr_data->compression = png->p_IHDR->p_data[10];
    ihdr_data->filter = png->p_IHDR->p_data[11];
    ihdr_data->interlace = png->p_IHDR->p_data[12];


    U8 *ihdr_buf = malloc(4 * png->p_IHDR->length);
    U8 *idat_buf = malloc(4 * png->p_IDAT->length);
    U8 *iend_buf = malloc(4);

    int index = 0;
    for(int i = 0; i < 4; i++){
           ihdr_buf[i] = png->p_IHDR->type[i];
           idat_buf[i] = png->p_IDAT->type[i];
           iend_buf[i] = png->p_IEND->type[i];
    }
    index = 0;
    for(int i = 4; i < 4 + png->p_IHDR->length; i++, index++){
           ihdr_buf[i] = png->p_IHDR->p_data[index];
    }
    index = 0;
    for(int i = 4; i < 4 + png->p_IDAT->length; i++, index++){
           idat_buf[i] = png->p_IDAT->p_data[index];
    }

    U32 ihdr_crc_calc = crc(ihdr_buf, 4 + DATA_IHDR_SIZE);
    U32 idat_crc_calc = crc(idat_buf, 4 + png->p_IDAT->length);
    U32 iend_crc_calc = crc(iend_buf, 4);

    // printf("%x %x %x\n", ihdr_crc_calc, idat_crc_calc, iend_crc_calc);
    // printf("%x %x %x\n", png->p_IHDR->crc, png->p_IDAT->crc, png->p_IEND->crc);

    if(png > 0){
        printf("%s: %d x %d\n", pngName, width, height);
        if(ihdr_crc_calc != png->p_IHDR->crc){
            printf("IHDR chunk CRC error: computed %x, expected %x\n", ihdr_crc_calc, png->p_IHDR->crc);
        }
        if(idat_crc_calc != png->p_IDAT->crc){
            printf("IDAT chunk CRC error: computed %x, expected %x\n", idat_crc_calc, png->p_IDAT->crc);
        }
        if(iend_crc_calc != png->p_IEND->crc){
            printf("IEND chunk CRC error: computed %x, expected %x\n", iend_crc_calc, png->p_IEND->crc);
        }
    }
    else{
        printf("%s: NOT a PNG file\n", pngName);
    }

    freePNG(png);

    free(ihdr_data);

    free(ihdr_buf);
    free(idat_buf);
    free(iend_buf);

    fclose(pngCheck);
    return 0;
}
