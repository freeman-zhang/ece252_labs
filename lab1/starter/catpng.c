#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                */
#include <errno.h>              /* for errno                   */
#include <string.h>             /* for strcpy                  */
#include "png_util/crc.h"       /* for crc()                   */
#include "png_util/lab_png.h"   /* simple PNG data structures  */
#include "png_util/zutil.h"     /* for mem_def() and mem_inf() */

#define BASE 256

/* Function to calculate values because the one from math.h doesn't work */
int power(int base, int exp){
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

int is_png(U8 *buf){
    U8 signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    U8 header[8];
    for(int i = 0; i < 8; i++){
        header[i] = buf[i];
    }
    return !memcmp(signature, header, sizeof(signature));
}

int get_png_width(struct chunk *ihdr){
    return getValue(ihdr->p_data, BASE, 0, 4);
}

int get_png_height(struct chunk *ihdr){
    return getValue(ihdr->p_data, BASE, 4, 8);
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
		return NULL;
    }
    memset(buffer, 0, buffer_size);
    fread(buffer, buffer_size, 1, png);
    int isPNG = is_png(buffer);
    if(!isPNG){
        return NULL;
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
    // printf("blah %x\n", iend_crc);

    U8 *buf = (U8 *)malloc(sizeof(U8) * buffer_size);
    for(int i = 0; i < buffer_size; i++){
        buf[i] = buffer[i];
    }

    // retVal->buffer = buf;
    retVal->p_IHDR = ihdr;
    retVal->p_IDAT = idat;
    retVal->p_IEND = iend;
	
	free(buf);
	free(buffer);

    return retVal;
}

int main(int argc, char *argv[]){
    /* Get path for png */
    if (argc == 1) {
        printf("Usage: %s png_file\n", argv[0]);
        return -1;
    }

    simple_PNG_p pngs[argc - 1];
    for(int i = 1; i < argc; i++){
        pngs[i - 1] = createPNG(argv[i]);
        if(pngs[i - 1] == NULL){
            printf("Couldnt find png\n");
            return -1;
        }
    }

    simple_PNG_p cat = malloc(3 * sizeof(struct simple_PNG));

    int width, height = 0;

    width = get_png_width(pngs[0]->p_IHDR);
    // printf("Width: %d\n", width);

    for(int i = 0; i < argc - 1; i++){
        height += get_png_height(pngs[i]->p_IHDR);
    }

    // printf("Height: %d\n", height);

    int final_size = height * (width * 4 + 1);
    U8 *final_buffer = malloc(final_size);
    U64 output_length = 0;
    int ret = 0;
    int offset = 0;
    for(int i = 0; i < argc - 1; i++){
        ret = mem_inf(final_buffer + (offset * sizeof(U8)), &output_length, pngs[i]->p_IDAT->p_data, pngs[i]->p_IDAT->length);
        if (ret == 0) { /* success */
        // printf("original len = %d, len_def = %lu, len_inf = %lu\n", \
               final_size, pngs[i]->p_IDAT->length, output_length);
        } else { /* failure */
            // fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
        }
        offset += output_length;
    }

    int decompressed_size = height * (4 * width + 1);
    U8 *decompressed_buffer = malloc(decompressed_size);
    U64 new_size = 0;

    ret = mem_def(decompressed_buffer, &new_size, final_buffer, offset, Z_DEFAULT_COMPRESSION);
    if (ret == 0) { /* success */
        // printf("original len = %d, len_def = %lu\n");
    } else { /* failure */
        // fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
        return ret;
    }

    int marker = 0;

    int new_buffer_size = PNG_SIG_SIZE + (3 * CHUNK_LEN_SIZE) + (3 * CHUNK_TYPE_SIZE) + (3 * CHUNK_CRC_SIZE) + DATA_IHDR_SIZE + new_size;

    U8 *ihdr_buf = malloc(4 + DATA_IHDR_SIZE);
    U8 *idat_buf = malloc(4 + new_size);
    U8 *iend_buf = malloc(4);

    U8 *new_height = insertValue(height, 4);
    U8 *new_ihdr_data = malloc(DATA_IHDR_SIZE * sizeof(U8));
    memcpy(new_ihdr_data, decompressed_buffer + PNG_SIG_SIZE + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE, DATA_IHDR_SIZE * sizeof(U8));
    memcpy(new_ihdr_data + 4, new_height, 4 * sizeof(U8));

    for(int i = 0; i < 4; i++){
           ihdr_buf[i] = pngs[0]->p_IHDR->type[i];
           idat_buf[i] = pngs[0]->p_IDAT->type[i];
           iend_buf[i] = pngs[0]->p_IEND->type[i];
    }
    int index = 0;
    for(int i = 4; i < 4 + DATA_IHDR_SIZE; i++, index++){
           ihdr_buf[i] = new_ihdr_data[index];
    }
    index = 0;
    for(int i = 4; i < 4 + new_size; i++, index++){
           idat_buf[i] = decompressed_buffer[index];
    }

    // U32 ihdr_crc_calc = crc(ihdr_buf, 4 + DATA_IHDR_SIZE);
    U32 idat_crc_calc = crc(idat_buf, 4 + new_size);
    U32 iend_crc_calc = crc(iend_buf, 4);

    U8 concat_png[new_buffer_size];

    U8 signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    memcpy(concat_png, signature, sizeof(signature));
    marker += PNG_SIG_SIZE;

    U8 *new_ihdr_length = insertValue(13, 4);
    memcpy(concat_png + marker, new_ihdr_length, sizeof(new_ihdr_length));
    marker += CHUNK_LEN_SIZE;

    memcpy(concat_png + marker, pngs[0]->p_IHDR->type, sizeof(pngs[0]->p_IHDR->type));
    marker += CHUNK_TYPE_SIZE;

    memcpy(concat_png + marker, pngs[0]->p_IHDR->p_data, pngs[0]->p_IHDR->length);

    memcpy(concat_png + marker + 4, new_height, 4 * sizeof(U8));
    marker += DATA_IHDR_SIZE;
    U32 ihdr_crc_calc = crc(concat_png + PNG_SIG_SIZE + CHUNK_LEN_SIZE, 4 + DATA_IHDR_SIZE);

    U8 *new_ihdr_crc = insertValue(ihdr_crc_calc, 4);
    
    memcpy(concat_png + marker, new_ihdr_crc, 4 * sizeof(U8));
    marker += CHUNK_CRC_SIZE;

    U8 *new_idat_length = insertValue(new_size, 4);
    memcpy(concat_png + marker, new_idat_length, 4 * sizeof(U8));
    marker += CHUNK_LEN_SIZE;

    memcpy(concat_png + marker, pngs[0]->p_IDAT->type, sizeof(pngs[0]->p_IDAT->type));
    marker += CHUNK_TYPE_SIZE;

    memcpy(concat_png + marker, decompressed_buffer, new_size);
    marker += new_size;

    U8 *new_idat_crc = insertValue(idat_crc_calc, 4);
    memcpy(concat_png + marker, new_idat_crc, 4 * sizeof(U8));
    marker += CHUNK_CRC_SIZE;

    U8 *new_iend_length = insertValue(0, 4);
    memcpy(concat_png + marker, new_iend_length, 4 * sizeof(U8));
    marker += CHUNK_LEN_SIZE;

    memcpy(concat_png + marker, pngs[0]->p_IEND->type, sizeof(pngs[0]->p_IDAT->type));
    marker += CHUNK_TYPE_SIZE;

    // IEND data but its empty so do nothing

    U8 *new_iend_crc = insertValue(iend_crc_calc, 4);
    memcpy(concat_png + marker, new_iend_crc, 4 * sizeof(U8));
    marker += CHUNK_CRC_SIZE;

    FILE *fp;
    fp = fopen("all.png", "w");
    fwrite(concat_png, 1, sizeof(concat_png), fp);

    fclose(fp);
	
	//deallocating memory
	free(new_height);
	free(final_buffer);

	free(cat);
	free(ihdr_buf);
	free(idat_buf);
	free(iend_buf);
	free(new_ihdr_data);
	
	free(pngs);
	
    return 0;
}
