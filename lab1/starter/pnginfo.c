#include <stdio.h>                  /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                */
#include <errno.h>              /* for errno                   */
#include <string.h>             /* for strcpy                  */
#include <math.h>
//#include "png_util/zutil.c"     /* for mem_def() and mem_inf() */
#include "png_util/crc.c"       /* for crc()                   */
#include "png_util/lab_png.h"   /* simple PNG data structures  */
// #include "ls/ls_fname.c"
// #include "ls/ls_ftype.c"

#define BUFFER_LENGTH 255

int power(int base, int exp){
    int i = 1;
    int result = 1;
    for(int i = 0; i < exp; i++){
        result *= base;
    }
    return result;
}

U32 getValue(unsigned char *buf, int base, int start, int end){
    int retVal = 0;
    int exp = 0;

    for(int i = start; i > end; i--){
        retVal += buf[i] * power(base, exp);
        exp++;
        printf("%x ", buf[i]);
    }
    printf("\n");
    return retVal;
}

int main(int argc, char *argv[])
{
    int i;

    /* Get path for png */
    if (argc == 1) {
        printf("Usage: %s png_file\n", argv[0]);
        return -1;
    }


    U8 pngPath[255];
    strcpy(pngPath, argv[1]);

    printf("%s\n", pngPath);

    U8 buffer[BUFFER_LENGTH];
    U8 header[8];
    memset(buffer, 0, BUFFER_LENGTH);
                                                                                                                                                                                                 
    /* Open png */
    FILE *png = fopen(pngPath, "rt");
    if(png == NULL){
            printf("couldnt find png\n");
            return -1;
    }

    /* Read png into buffer */
    fread(buffer, sizeof(buffer), 1, png);

    U8 signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    int isPNG;

    /* Check header for png signature */
    for(int i = 0; i < 8; i++){
        header[i] = buffer[i];
    }
    if (isPNG = !memcmp(signature, header, sizeof(signature))){
        printf("this is a png\n");
    }
    printf("%d\n", isPNG);

    data_IHDR_p ihdr_data = malloc(sizeof(data_IHDR_p));

    /* Width = 16 - 19; height = 20 - 23 */
    int width, height, exp = 0;
    int base = 256;

    U8 ihdr_type[4];
    int index = 0;
    for(int i = 13; i < 17; i++, index++){
           ihdr_type[index] = buffer[i];
    }

    chunk_p ihdr = malloc(sizeof(chunk_p));
    ihdr->length = getValue(buffer, base, 12, 8);
    memcpy(ihdr->type, ihdr_type, sizeof(ihdr_type));
    ihdr->p_data = &buffer[18];


    width = getValue(buffer, base, 19, 15);
    height = getValue(buffer, base, 23, 19);

    ihdr_data->width = width;
    ihdr_data->height = height;
    ihdr_data->bit_depth = buffer[24];
    ihdr_data->color_type = buffer[25];
    ihdr_data->compression = buffer[26];
    ihdr_data->filter = buffer[27];
    ihdr_data->interlace = buffer[28];

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

    U32 ihdr_crc = getValue(buffer, base, 32, 28);
    // printf("%d\n", crc_val);
    ihdr->crc = ihdr_crc;

    U32 IDAT_length = getValue(buffer, base, 36, 32);

    printf("IDAT length: %d\n", IDAT_length);

    U8 idat_type[4];
    index = 0;
    for(int i = 37; i < 41; i++, index++){
            idat_type[index] = buffer[i];
    }

    chunk_p idat = malloc(sizeof(chunk_p));
    idat->length = IDAT_length;
    memcpy(idat->type, idat_type, sizeof(idat_type));
    idat->p_data = &buffer[42];

    //36 + IDAT_length + 8:
    //36: start of IDAT
    //IDAT_LENGTH: length of data
    //8: to skip over length and type bytes
    U32 IDAT_crc = getValue(buffer, base, 36 + IDAT_length + 8, 32 + IDAT_length + 8);
    idat->crc = IDAT_crc;

    U8 iend_type[4];
    for(int i = 46 + IDAT_length; i < 50 + IDAT_length; i++, index++){
            iend_type[index] = buffer[i];
    }
    chunk_p iend = malloc(sizeof(chunk_p));
    iend->length = 0;
    memcpy(iend->type, iend_type, sizeof(iend_type));
    iend->p_data = NULL;

    U32 IEND_crc = getValue(buffer, base, 54 + IDAT_length, 50 + IDAT_length);
    iend->crc = IEND_crc;

    U8 *ihdr_buf = malloc(sizeof(ihdr->type) + sizeof(*ihdr->p_data));
    U8 *idat_buf = malloc(sizeof(idat->type) + sizeof(*idat->p_data));
    U8 *iend_buf = malloc(sizeof(iend->type));

    memcpy(ihdr_buf, ihdr->type, 4 * sizeof(U8));
    memcpy(ihdr_buf + 4 * sizeof(U8), ihdr->p_data, ihdr->length * sizeof(U8));

    memcpy(idat_buf, idat->type, 4 * sizeof(U8));
    memcpy(idat_buf + 4 * sizeof(U8), idat->p_data, idat->length * sizeof(U8));

    memcpy(iend_buf, iend->type, 4 * sizeof(U8));
    memcpy(iend_buf + 4 * sizeof(U8), iend->p_data, iend->length * sizeof(U8));


    U32 ihdr_crc_calc = crc(ihdr_buf, 4 + ihdr->length);
    U32 idat_crc_calc = crc(idat_buf, 4 + idat->length);
    U32 iend_crc_calc = crc(iend_buf, 4 + iend->length);

    printf("%x, %x, %x\n", ihdr_crc_calc, idat_crc_calc, iend_crc_calc);
    printf("%x, %x, %x\n", ihdr->crc, idat->crc, iend->crc);

    /* On success */
    if(isPNG){
        printf("%s: %d x %d\n", pngName, width, height);
        /*
        if(!crc_val){
            printf("IDAT chunk CRC error: computed %s, expected %s\n", "test", "test");
        }
        */
    }
    else{
        printf("%s: NOT a PNG file\n", pngName);
    }

    printf("%x\n", buffer[47]);
    return 0;
}



