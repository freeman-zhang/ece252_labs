#include <stdio.h>  /* printf needs to include this header file */
#include <stdlib.h> /* for malloc()                 */
#include <unistd.h> /* for getopt                   */
#include <getopt.h> /* to get rid of optarg linting */
#include <string.h>
//#include <curl/curl.h>        /* for cURL                     */
#include <pthread.h>          /* for pthread                  */
#include "png_util/lab_png.h" /* simple PNG data structures   */
//#include "cURL/curl_util.h"   /* for header and write cb      */

typedef struct char_queue
{
    int front;
    int rear;
    int count;
    int max;
    char **url;
} * char_queue_p;

int empty(png_queue_p queue)
{
    return queue->count == 0;
}

char *dequeue(char_queue_p queue)
{
    char *retVal;
    if (!empty(queue))
    {
        retVal = &queue->url[queue->front++];

        if (queue->front == queue->max)
        {
            queue->front = 0;
        }
        queue->count--;
    }
    else
    {
        retVal = NULL;
    }
    return retVal;
}

int enqueue(char_queue_p queue, struct recv_buf buf)
{
    if (!full(queue))
    {
        if (queue->rear == queue->max - 1)
        {
            queue->rear = -1;
        }
        queue->rear = queue->rear + 1;
        memcpy(queue->url[queue->rear].entry, buf.buf, buf.max_size);
        queue->url[queue->rear].number = buf.seq;

        queue->count++;
    }
    else
    {
        return -1;
    }
    return 0;
}

void *crawler(void *url)
{
    // dq a url from frontier

    // curl call to url

    // use libxml to traverse page,
    // check links against pages visted:
    // if on list, then ignore
    // if not on list, then add to frontier

    // check pngs against pngs list:
    // if on list, ignore
    // if not on list and pngs list not full, add to png list

    // exit conditions:
    // All threads are done and frontier empty
    // pngs full

    // repeat

    // on exit, cleanup
}

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    pid_t pid = getpid();
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if (res == CURLE_OK)
    {
        printf("Response code: %ld\n", response_code);
    }

    if (response_code >= 400)
    {
        fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if (res == CURLE_OK && ct != NULL)
    {
        printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    }
    else
    {
        fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if (strstr(ct, CT_HTML))
    {
        // check links against pages visted:
        // if on list, then ignore
        // if not on list, then add to frontier
    }
    else if (strstr(ct, CT_PNG))
    {
        // check pngs against pngs list:
        // if on list, ignore
        // if not on list and pngs list not full, add to png list
    }

    return 0;
}

char_queue_p frontier;

int main(int argc, char **argv)
{
    int c;
    int num_threads = 1;
    int num_pngs = 50;
    char logfile[100] = "";
    /* Maybe replace this with a call to main_getopt.c */
    while ((c = getopt(argc, argv, "t:m:v:")) != -1)
    {
        switch (c)
        {
        case 't':
            num_threads = strtoul(optarg, NULL, 10);
            if (num_threads <= 0)
            {
                fprintf(stderr, "%s: -t > 0 -- 't'\n", argv[0]);
                return -1;
            }
            break;
        case 'm':
            num_pngs = strtoul(optarg, NULL, 10);
            if (num_pngs <= 0)
            {
                fprintf(stderr, "%s: -m > 0 -- 'n'\n", argv[0]);
                return -1;
            }
            break;
        case 'v':
            strcpy(logfile, optarg);
            break;
        default:
            return -1;
        }
    }

    frontier = malloc(sizeof(4 * int) + sizeof(100 * 256 * char));

    char seedurl[256];
    strcpy(seedurl, argv[argc - 1]);

    // printf("num threads = %d\n", num_threads);
    // printf("num pngs = %d\n", num_pngs);
    // printf("logfile = %s\n", logfile);
    // printf("seed = %s\n", seedurl);

    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = easy_handle_init(&recv_buf, seedurl);
    if (curl_handle == NULL)
    {
        fprintf(stderr, "Curl initialization failed. Exiting...\n");
        curl_global_cleanup();
        abort();
    }

    res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        cleanup(curl_handle, &recv_buf);
        exit(1);
    }

    process_data(curl_handle, &recv_buf);

    //curl
    //use libxml, traverse the seed url, set up frontier
    //add seed url to pages visited

    //create threads
    // pthread_t crawlers[num_threads];
    // for (int i = 0; i < num_threads; i++)
    // {
    //     pthread_create(&crawlers[i], NULL, crawler, NULL);
    // }
    return 0;
}