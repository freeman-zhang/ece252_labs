#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                 */
#include <errno.h>              /* for errno                    */
#include <string.h>             /* for strcpy                   */
#include <unistd.h>             /* for getopt                   */
#include <getopt.h>             /* to get rid of optarg linting */
#include <curl/curl.h>          /* for cURL                     */
#include <pthread.h>            /* for pthread                  */
#include "png_util/crc.h"       /* for crc()                    */
#include "png_util/lab_png.h"   /* simple PNG data structures   */
#include "png_util/zutil.h"     /* for mem_def() and mem_inf()  */
#include "cURL/curl_util.h"

#define final_height 300
#define final_width 400
#define num_pngs 50
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */

volatile int num_found = 0;
simple_PNG_p pngs[num_pngs] = {NULL};

void * run(void * argument){
    int* retVal = malloc(sizeof(int));
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    RECV_BUF recv_buf;

    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, argument);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb_curl3);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_buf);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb_curl);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&recv_buf);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    }
    while(num_found < 50){
        recv_buf_init(&recv_buf, BUF_SIZE);
        res = curl_easy_perform(curl);
        if(res == CURLE_OK){
            if(!pngs[recv_buf.seq]){
                pngs[recv_buf.seq] = createPNG(recv_buf.buf, recv_buf.size);
                num_found++;
            }
        }
        else{
            printf("ERROR: curl failed; aborting\n");
            *retVal = -1;
            pthread_exit(retVal);
        }
    }
    *retVal = 0;
    pthread_exit(retVal);
}

int main(int argc, char **argv){
    int c;
    int num_threads = 1;
    int img_num = 1;
    /* Maybe replace this with a call to main_getopt.c */
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
            num_threads = strtoul(optarg, NULL, 10);
            if (num_threads <= 0) {
                    fprintf(stderr, "%s: -t > 0 -- 't'\n", argv[0]);
                    return -1;
                }
        break;
        case 'n':
            img_num = strtoul(optarg, NULL, 10);
            if (img_num <= 0 || img_num > 3) {
                fprintf(stderr, "%s: -n must be followed by: 1, 2, or 3 -- 'n'\n", argv[0]);
                return -1;
            }
        break;
        default:
            return -1;
        }
    }

    char url[100] = "http://ece252-1.uwaterloo.ca:2520/image?img=";
	//string concatentation of the image number to the url
    char img_num_char[2];
    sprintf(img_num_char, "%d", img_num);
	strcat(url, img_num_char);

    pthread_t threads[num_threads];
    void *vr[num_threads];

    for(int i = 0; i < num_threads; i++){
        pthread_create(&threads[i], NULL, run, url);
    }
    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], &vr[i]);
        if(*(int*)vr[i] == -1){
            printf("ERROR: thread %d failed\n", *(int*)vr[i]);
        }
    }
	
	if(catPNG(pngs, num_pngs, final_height, final_width) != 0){
        printf("Error occured when concatenating PNGs\n");
        return -1;
    }
	
	for(int i = 0; i < num_pngs; i++){
        freePNG(pngs[i]);
    }
    curl_global_cleanup();
    return 0;
}