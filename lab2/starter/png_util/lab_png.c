#include <string.h>
#include <stdlib.h>

#include "lab_png.h"

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

void freePNG(simple_PNG_p png){
    free(png->p_IHDR->p_data);
    free(png->p_IHDR);
    free(png->p_IDAT->p_data);
    free(png->p_IDAT);
    free(png->p_IEND);
    free(png);
}

U8 *insertValue(U32 value, int size){
    U8 *retVal = malloc(size);
    // printf("blah %x\n", value);
    for(int i = size - 1; i >= 0; i--){
        retVal[i] = value % 256;
        value /= 256;
    }
    return retVal;
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
    U8 *ihdrData = malloc(ihdr->length);
    memcpy(ihdrData, buffer + ihdr_start + 8, ihdr->length);
    ihdr->p_data = ihdrData;
    ihdr->crc = ihdr_crc;

    idat->length = idat_length;
    memcpy(idat->type, idat_type, sizeof(idat_type));
    U8 *idatData = malloc(idat->length);
    memcpy(idatData, buffer + idat_start + 8, idat->length);
    idat->p_data = idatData;
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

    free(buffer);

    fclose(png);

    return retVal;
}