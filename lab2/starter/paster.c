#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                 */
#include <errno.h>              /* for errno                    */
#include <string.h>             /* for strcpy                   */
#include <unistd.h>             /* for getopt                   */
#include <getopt.h>             /* to get rid of optarg linting */
#include <curl/curl.h>          /* for cURL                     */
#include "png_util/crc.h"       /* for crc()                    */
#include "png_util/lab_png.h"   /* simple PNG data structures   */
#include "png_util/zutil.h"     /* for mem_def() and mem_inf()  */
#include "cURL/curl_util.h"

size_t curl_header_cb(char *p_recv, size_t size, size_t nmeb, void *userdata){

    return 0;
}

size_t curl_data_cb(char *p_recv, size_t size, size_t nmemb, void *p_userdata){

    return 0;
}

int main(int argc, char **argv){
    int c;
    int num_threads = 1;
    int img_num = 1;
    char *str = "option requires an argument";
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
    // printf("%d, %d\n", num_threads, img_num);
    char *url = "http://ece252-1.uwaterloo.ca:2520/image?img=1";

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb_curl);
        
        res = curl_easy_perform(curl);
    }
    if(res != CURLE_OK){
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_global_cleanup();
    return 0;
}