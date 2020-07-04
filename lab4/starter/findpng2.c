#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                 */
#include <unistd.h>             /* for getopt                   */
#include <getopt.h>             /* to get rid of optarg linting */
#include <curl/curl.h>          /* for cURL                     */
#include <pthread.h>            /* for pthread                  */
#include "png_util/lab_png.h"   /* simple PNG data structures   */
#include "cURL/curl_util.h"     /* for header and write cb      */

int main(int argc, char **argv){
    int c;
    int num_threads = 1;
    int num_urls = 50;
    char logfile = "";
    /* Maybe replace this with a call to main_getopt.c */
    while ((c = getopt (argc, argv, "t:m:")) != -1) {
        switch (c) {
        case 't':
            num_threads = strtoul(optarg, NULL, 10);
            if (num_threads <= 0) {
                    fprintf(stderr, "%s: -t > 0 -- 't'\n", argv[0]);
                    return -1;
                }
        break;
        case 'm':
            num_urls = strtoul(optarg, NULL, 10);
            if (num_urls <= 0) {
                fprintf(stderr, "%s: -m > 0 -- 'n'\n", argv[0]);
                return -1;
            }
        break;
        default:
            return -1;
        }
    }

    return 0;
}