#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                */
#include <errno.h>              /* for errno                   */
#include <string.h>             /* for strcpy                  */
#include "png_util/crc.c"       /* for crc()                   */
#include "png_util/lab_png.h"   /* simple PNG data structures  */

#define BASE 256

/* Function to calculate values because the one from math.h doesn't work */
int power(int base, int exp){
    int i = 1;
    int result = 1;
    for(int i = 0; i < exp; i++){
        result *= base;
    }
    return result;
}

/* Used with power() to calculate values from buffer */
U32 getValue(unsigned char *buf, int base, int start, int end){
    int retVal = 0;
    int exp = end - start - 1;

    for(int i = start; i < end; i++){
        retVal += buf[i] * power(base, exp);
        exp--;
    }
    return retVal;
}

/* Looks at file size to see how much space needs to be allocated to buffer */
U32 getBufferSize(U8 *path){
    FILE *png = fopen(path, "rb");
    if(png == NULL){
        return -1;
    }

    fseek(png, 0L, SEEK_END);
    return ftell(png);
}

/* Checks if the file's header matches that of a png */
int is_png(U8 *buf){
    U8 signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    U8 header[8];
    for(int i = 0; i < 8; i++){
        header[i] = buf[i];
    }
    return !memcmp(signature, header, sizeof(signature));
}

int get_png_width(struct chunk *ihdr){
    return getValue(ihdr->p_data, 256, 0, 4);
}

int get_png_height(struct chunk *ihdr){
    return getValue(ihdr->p_data, 256, 4, 8);
}

simple_PNG_p createPNG(U8 *path){
    simple_PNG_p retVal = malloc(3 * sizeof(struct simple_PNG));
    U32 buffer_size = getBufferSize(path);
    U8* buffer = malloc(buffer_size);
    FILE *png = fopen(path, "rb");
    if(png == NULL){
        retVal->errCode = -1;
        return retVal;
    }
    memset(buffer, 0, buffer_size);
    fread(buffer, buffer_size, 1, png);
    int isPNG = is_png(buffer);
    if(!isPNG){
        retVal->errCode = -2;
        return retVal;
    }

    chunk_p ihdr = malloc(sizeof(struct chunk));
    chunk_p idat = malloc(sizeof(struct chunk));
    chunk_p iend = malloc(sizeof(struct chunk));

    /* Getting values for ihdr chunk */
    int ihdr_start = 8;
    U8 ihdr_type[4];
    U32 ihdr_length = getValue(buffer, BASE, ihdr_start, ihdr_start + 4);
    int index = 0;
    /* ihdr type bytes should be 12 to 15 always */
    for(int i = ihdr_start + 4; i < ihdr_start + 8; i++, index++){
        ihdr_type[index] = buffer[i];
    }
    U32 ihdr_crc = getValue(buffer, BASE, ihdr_start + ihdr_length + 8, ihdr_start + ihdr_length + 12);

    /* Getting values for idat chunk */
    int idat_start = ihdr_start + ihdr_length + 12;
    U8 idat_type[4];
    U32 idat_length = getValue(buffer, BASE, idat_start, idat_start + 4);
    index = 0;
    for(int i = idat_start + 4; i < idat_start + 8; i++, index++){
        idat_type[index] = buffer[i];
    }
    U32 idat_crc = getValue(buffer, BASE, idat_start + idat_length + 8, idat_start + idat_length + 12);

    /* Getting values for iend chunk */
    int iend_start = idat_start + idat_length + 12;
    U8 iend_type[4];
    U32 iend_length = getValue(buffer, BASE, iend_start, iend_start + 4);
    index = 0; 
    for(int i = iend_start + 4; i < iend_start + 8; i++, index++){
        iend_type[index] = buffer[i];
    }
    U32 iend_crc = getValue(buffer, BASE, iend_start + 8, iend_start + 12);

    ihdr->length = ihdr_length;
    memcpy(ihdr->type, ihdr_type, sizeof(ihdr_type));
    ihdr->p_data = &buffer[ihdr_start + 8];
    ihdr->crc = ihdr_crc;

    idat->length = idat_length;
    memcpy(idat->type, idat_type, sizeof(idat_type));
    idat->p_data = &buffer[idat_start + 8];
    idat->crc = idat_crc;

    iend->length = iend_length;
    memcpy(iend->type, iend_type, sizeof(iend_type));
    iend->p_data = NULL;
    iend->crc = iend_crc;

    U8 *buf = (U8 *)malloc(sizeof(U8) * buffer_size);
    for(int i = 0; i < buffer_size; i++){
        buf[i] = buffer[i];
    }

    retVal->p_IHDR = ihdr;
    retVal->p_IDAT = idat;
    retVal->p_IEND = iend;

    fclose(png);

    return retVal;
}

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

    free(png->p_IDAT);
    free(png->p_IHDR);
    free(png->p_IHDR);

    free(ihdr_data);

    free(ihdr_buf);
    free(idat_buf);
    free(iend_buf);

    fclose(pngCheck);
    return 0;
}
