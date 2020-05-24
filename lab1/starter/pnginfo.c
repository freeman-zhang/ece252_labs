#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                */
#include <errno.h>              /* for errno                   */
#include <string.h>             /* for strcpy                  */
#include "png_util/crc.c"       /* for crc()                   */
#include "png_util/lab_png.h"   /* simple PNG data structures  */

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
            printf("couldnt find png\n");
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

int main(int argc, char *argv[]){
    /* Get path for png */
    if (argc == 1) {
        printf("Usage: %s png_file\n", argv[0]);
        return -1;
    }

    U8 pngPath[255];
    strcpy(pngPath, argv[1]);

    /* Open png */
    FILE *png = fopen(pngPath, "rb");
    if(png == NULL){
            printf("couldnt find png\n");
            return -1;
    }

    U32 buffer_size = getBufferSize(pngPath);
    U8 buffer[buffer_size];
    U8 header[8];
    for(int i = 0; i < 8; i++){
        header[i] = buffer[i];
    }
    memset(buffer, 0, buffer_size);

    /* Read png into buffer */
    fread(buffer, sizeof(buffer), 1, png);

    /* Get file name as path */
    char *pngName = pngPath;
    char *ssc;
    int l = 0;
    ssc = strstr(pngName, "/");
    do{
        l = strlen(ssc) + 1;
        pngName = &pngName[strlen(pngName)-l+2];
        ssc = strstr(pngName, "/");
    }while(ssc);

    int isPNG = is_png(buffer);
    if (!isPNG){
        printf("%s: NOT a PNG file\n", pngName);
        return -1;
    }

    data_IHDR_p ihdr_data = malloc(sizeof(data_IHDR_p));
    chunk_p ihdr = malloc(sizeof(chunk_p));
    chunk_p idat = malloc(sizeof(chunk_p));
    chunk_p iend = malloc(sizeof(chunk_p));

    /* Since each index consists of 2 hex characters: 16 * 16 = 256 */
    int base = 256;

    /* Getting values for ihdr chunk */
    int ihdr_start = 8;
    U8 ihdr_type[4];
    U32 ihdr_length = getValue(buffer, base, ihdr_start, ihdr_start + 4);
    int index = 0;
    /* ihdr type bytes should be 12 to 15 always */
    for(int i = ihdr_start + 4; i < ihdr_start + 8; i++, index++){
        ihdr_type[index] = buffer[i];
    }
    U32 ihdr_crc = getValue(buffer, base, ihdr_start + ihdr_length + 8, ihdr_start + ihdr_length + 12);

    /* Getting values for idat chunk */
    int idat_start = ihdr_start + ihdr_length + 12;
    U8 idat_type[4];
    U32 idat_length = getValue(buffer, base, idat_start, idat_start + 4);
    index = 0;
    for(int i = idat_start + 4; i < idat_start + 8; i++, index++){
        idat_type[index] = buffer[i];
    }
    U32 idat_crc = getValue(buffer, base, idat_start + idat_length + 8, idat_start + idat_length + 12);

    /* Getting values for iend chunk */
    int iend_start = idat_start + idat_length + 12;
    U8 iend_type[4];
    U32 iend_length = getValue(buffer, base, iend_start, iend_start + 4);
    index = 0; 
    for(int i = iend_start + 4; i < iend_start + 8; i++, index++){
        iend_type[index] = buffer[i];
    }
    U32 iend_crc = getValue(buffer, base, iend_start + 8, iend_start + 12);

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

    int width, height= 0;

    width = get_png_width(ihdr);
    height = get_png_height(ihdr);

    ihdr_data->width = width;
    ihdr_data->height = height;
    ihdr_data->bit_depth = buffer[24];
    ihdr_data->color_type = buffer[25];
    ihdr_data->compression = buffer[26];
    ihdr_data->filter = buffer[27];
    ihdr_data->interlace = buffer[28];


    U8 *ihdr_buf = malloc(4 + ihdr->length);
    U8 *idat_buf = malloc(4 + idat->length);
    U8 *iend_buf = malloc(4);

    for(int i = 0; i < 4; i++){
           ihdr_buf[i] = ihdr->type[i];
           idat_buf[i] = idat->type[i];
           iend_buf[i] = iend->type[i];
    }
    index = 0;
    for(int i = 4; i < 4 + ihdr->length; i++, index++){
           ihdr_buf[i] = ihdr->p_data[index];
    }
    index = 0;
    for(int i = 4; i < 4 + idat->length; i++, index++){
           idat_buf[i] = idat->p_data[index];
    }

    U32 ihdr_crc_calc = crc(ihdr_buf, 4 + ihdr->length);
    U32 idat_crc_calc = crc(idat_buf, 4 + idat->length);
    U32 iend_crc_calc = crc(iend_buf, 4);

    if(isPNG){
        printf("%s: %d x %d\n", pngName, width, height);
        if(ihdr_crc_calc != ihdr->crc){
            printf("IHDR chunk CRC error: computed %x, expected %x\n", ihdr_crc_calc, ihdr->crc);
        }
        if(idat_crc_calc != idat->crc){
            printf("IDAT chunk CRC error: computed %x, expected %x\n", idat_crc_calc, idat->crc);
        }
        if(iend_crc_calc != iend->crc){
            printf("IEND chunk CRC error: computed %x, expected %x\n", iend_crc_calc, iend->crc);
        }
    }
    else{
        printf("%s: NOT a PNG file\n", pngName);
    }

    free(ihdr_data);
    free(ihdr);
    free(idat);
    free(iend);

    free(ihdr_buf);
    free(idat_buf);
    free(iend_buf);

    fclose(png);
    return 0;
}