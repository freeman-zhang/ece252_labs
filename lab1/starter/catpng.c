#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                */
#include <errno.h>              /* for errno                   */
#include <string.h>             /* for strcpy                  */
#include "png_util/crc.h"       /* for crc()                   */
#include "png_util/lab_png.h"   /* simple PNG data structures  */
#include "png_util/zutil.h"     /* for mem_def() and mem_inf() */

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

    int width, height = 0;

    width = get_png_width(pngs[0]->p_IHDR);

    for(int i = 0; i < argc - 1; i++){
        height += get_png_height(pngs[i]->p_IHDR);
    }

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
            fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
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
        fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
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

    for(int i = 0; i < argc - 1; i++){
        freePNG(pngs[i]);
    }

    free(new_height);
    free(new_ihdr_length);
    free(new_ihdr_crc);
    free(new_idat_length);
    free(new_idat_crc);
    free(new_iend_length);
    free(new_iend_crc);

    free(ihdr_buf);
    free(idat_buf);
    free(iend_buf);
    free(decompressed_buffer);
    free(final_buffer);
    free(new_ihdr_data);

    return 0;
}
