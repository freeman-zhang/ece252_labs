#include <stdio.h>  /* printf needs to include this header file */
#include <stdlib.h> /* for malloc()                 */
#include <unistd.h> /* for getopt                   */
#include <getopt.h> /* to get rid of optarg linting */
#include <string.h>
#include <curl/curl.h>        /* for cURL                     */
#include <pthread.h>          /* for pthread                  */
#include <search.h>           /* for hcreate */
#include "png_util/lab_png.h" /* simple PNG data structures   */
#include "cURL/curl_util.h"   /* for header and write cb      */
#include <semaphore.h>

#define _GNU_SOURCE
#define CT_PNG "image/png"
#define CT_HTML "text/html"
#define VISITED_HT_SIZE 100

typedef struct char_queue
{
    int front;
    int rear;
    int count;
    int max;
    char urls[100][100];
} * char_queue_p;

//global vars
char_queue_p frontier;
struct hsearch_data *png_list;
struct hsearch_data *visited;
int png_count = 0;
int num_pngs = 50;
char logfile[100] = "";

int empty(char_queue_p queue)
{
    return queue->count == 0;
}

char* dequeue(char_queue_p queue)
{
    char *returl;
    if (!empty(queue))
    {
        for(int i = 0; i < strlen(queue->urls[queue->front]); ++i){
            returl[i] = queue->urls[queue->front][i];
        }   

        queue->front++;
        if (queue->front == queue->max)
        {
            queue->front = 0;
        }
        queue->count--;
    }
    else
    {
        return NULL;
    }
    return returl;
}

int enqueue(char_queue_p queue, char* url)
{
  
    if (empty(queue))
    {
        queue->front = 0;
        queue->rear = 0;
    }
    else{
        queue->rear = queue->rear + 1;
    }
    
    strcpy(queue->urls[queue->rear], url);
    queue->count++;
    return 0;
}

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    //pid_t pid = getpid();
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

    ENTRY hurl, *hret;
    char *url = NULL; 
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    //find_http(p_recv_buf->buf, p_recv_buf->size, 1, url); 
    hurl.key = url;
    int n = 0;
    if (strstr(ct, CT_HTML))
    {   
        printf("html yes \n");
        n = hsearch_r(hurl, FIND, &hret, visited);
        // if n is still 0, then the link was not found in ht
        if (!n)
        {
            //add to frontier;
            enqueue(frontier, url);
            //add to ht
            hurl.data = url;
            hsearch_r(hurl, ENTER, &hret, visited);

            //if logfile is not empty
            if (strcmp(logfile, ""))
            {
                //write to logfile;
                FILE *log;
                log = fopen(logfile, "a");
                fputs(url, log);
                fputs("\n",log);
                fclose(log);
            }
        }
       
    }
    else if (strstr(ct, CT_PNG))
    {
        printf("png yes \n");
        n = hsearch_r(hurl, FIND, &hret, png_list);
        // if n is still 0, then the png was not found in ht
        if (!n)
        {
            //add to ht
            if (png_count < num_pngs)
            {
                hurl.data = url;
                hsearch_r(hurl, ENTER, &hret, visited);
                png_count++;


                //write to png_urls.txt;
                FILE *png_file;
                png_file = fopen("png_urls.txt", "a");
                fputs(url, png_file);
                fputs("\n",png_file);
                fclose(png_file);
            }
        }
    }

    return 0;
}

void *crawler(void *ignore)
{
    
    while (!empty(frontier) ){
        
        CURL *curl_handle;
        CURLcode res;
        RECV_BUF recv_buf;
        curl_global_init(CURL_GLOBAL_DEFAULT);
        
        // dq a url from frontier
        printf("%d\n", empty(frontier));
        char *url = dequeue(frontier); 
        printf("checking %s\n", url);
        // curl call to url
        
        curl_handle = easy_handle_init(&recv_buf, url);

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
    }
    // exit conditions:
    // All threads are done and frontier empty
    // pngs full

    // repeat

    // on exit, cleanup

    pthread_exit(0);
}


int main(int argc, char **argv)
{
    int c;
    int num_threads = 1;
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
    //seedurl by taking last arg
    char seedurl[256];
    strcpy(seedurl, argv[argc - 1]);

    //frontier for pages to visit
    frontier = malloc(sizeof(int) * 4 + sizeof(char) * 100 * 256);

    //htable for png_list
    png_list = calloc(1, sizeof(png_list));
    hcreate_r(num_pngs, png_list);

    //htable for urls_visited
    visited = calloc(1, sizeof(visited));
    hcreate_r(VISITED_HT_SIZE, visited);

    // ENTRY hurl, *hret;
    // char link[100] = "link1";
    // hurl.key = link;
    // hurl.data = link;

    // hsearch_r(hurl, ENTER, &hret, png_list);

    // hurl.key = "link2";
    // hurl.data = "link2";
    // hsearch_r(hurl, ENTER, &hret, png_list);

    // hurl.key = "link1";

    // printf("current data = %s\n", (char *)hret->data);
    // hsearch_r(hurl, FIND, &hret, png_list);
    // printf("new data = %s\n", (char *)hret->data);
    // printf("CHECKPOINT\n");

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

    //create threads
    pthread_t crawlers[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        pthread_create(&crawlers[i], NULL, crawler, NULL);
        printf("created thread %d\n", i);
    }

    for (int i = 0; i < num_threads; i++)
    {   
        pthread_join(crawlers[i], NULL);
        printf("joined thread %d\n", i);
    }


    //char* printurl = dequeue(frontier); 
    //printf("link = %s\n", printurl);

   cleanup(curl_handle, &recv_buf);
    return 0;
}