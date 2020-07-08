#include <stdio.h>  /* printf needs to include this header file */
#include <stdlib.h> /* for malloc()                 */
#include <unistd.h> /* for getopt                   */
#include <getopt.h> /* to get rid of optarg linting */
#include <string.h>
#include <pthread.h>          /* for pthread                  */
#include <search.h>           /* for hcreate */
#include "png_util/lab_png.h" /* simple PNG data structures   */
#include "cURL/curl_util.h"   /* for header and write cb      */
//#include <semaphore.h>
#include <time.h>

#define CT_PNG "image/png"
#define CT_HTML "text/html"
//#define VISITED_HT_SIZE 100

#define _GNU_SOURCE

typedef struct char_queue
{
    int front;
    int rear;
    int count;
    char *urls[1000];
} * char_queue_p;

//global vars
char_queue_p frontier;
//struct hsearch_data *visited;
char *visited_array[400];
int png_count = 0;
int link_count = 0;
int num_pngs = 50;
char logfile[100] = "";
int anyone_running = 0;
struct timespec wait_time = {0};
//semaphores
pthread_mutex_t q_mutex;
pthread_mutex_t count_mutex;
pthread_mutex_t ht_mutex;
pthread_mutex_t log_mutex;
pthread_mutex_t png_mutex;
pthread_mutex_t running_mutex;
pthread_mutex_t link_mutex;

//function declarations
int find_http(char *buf, int size, int follow_relative_links, const char *base_url, CURL *curl_handle);
int check_link(CURL *curl_handle, RECV_BUF *p_recv_buf);
// int hcreate_r(size_t nel, struct hsearch_data *htab);
// int hsearch_r(ENTRY item, ACTION action, ENTRY **retval, struct hsearch_data *htab);
// void hdestroy_r(struct hsearch_data *htab);

int empty(char_queue_p queue)
{
    return queue->count == 0;
}

char *dequeue(char_queue_p queue)
{
    char *returl = NULL; 
    if (!empty(queue))
    {
        //printf("count = %d\n",queue->count);
        returl = malloc(strlen(queue->urls[queue->front]) + 1);
        strcpy(returl, queue->urls[queue->front]);
        //returl = strdup(queue->urls[queue->front]);
        //free(queue->urls[queue->front]);
        queue->front++;
        queue->count--;
    }
    //free(queue->urls[queue->front]);
    return returl;
}

int enqueue(char_queue_p queue, char *url)
{
    //printf("count = %d\n",queue->count);
    queue->urls[queue->rear] = malloc(strlen(url) + 1);
    strcpy(queue->urls[queue->rear], url);
    //queue->urls[queue->rear] = strdup(url);
    //free(url);
    //strcpy(queue->urls[queue->rear], url);
    queue->rear++;
    queue->count++;
    return 0;
}

int check_link(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    char *ct = NULL;

    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if (res != CURLE_OK || ct == NULL)
    {
        fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }
    char *checkurl = NULL;
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &checkurl);
    //printf("check: %s\n", url);
    if (strcmp(logfile, ""))
    {
        //write to logfile;
        pthread_mutex_lock(&log_mutex);
        FILE *log;
        log = fopen(logfile, "a");
        fputs(checkurl, log);
        fputs("\n", log);
        fclose(log);
        pthread_mutex_unlock(&log_mutex);
    }
    
    //printf("link: %s, type: %s\n", url, ct);

    if (strstr(ct, CT_HTML))
    {
        find_http(p_recv_buf->buf, p_recv_buf->size, 1, checkurl, curl_handle);
    }
    else if (strstr(ct, CT_PNG))
    {
        U8 * header;
        memcpy(&header, p_recv_buf, sizeof(header));
        if(is_png(header)){
            //printf("png yes \n");
            //write to png_urls.txt;
            
            pthread_mutex_lock(&count_mutex);
            if (png_count < num_pngs)
            {
                png_count++;
                pthread_mutex_unlock(&count_mutex);
                pthread_mutex_lock(&png_mutex);
                FILE *png_file;
                png_file = fopen("png_urls.txt", "a");
                fputs(checkurl, png_file);
                fputs("\n", png_file);
                fclose(png_file);
                pthread_mutex_unlock(&png_mutex);
            }
            else
            {
                pthread_mutex_unlock(&count_mutex);
            }
        }
    }
    //printf("done check\n");
    return 0;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url, CURL *curl_handle)
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar *)"//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;

    if (buf == NULL)
    {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset(doc, xpath);
    if (result)
    {
        nodeset = result->nodesetval;
        for (i = 0; i < nodeset->nodeNr; i++)
        {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if (follow_relative_links)
            {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *)base_url);
                xmlFree(old);
            }
            if (href != NULL && !strncmp((const char *)href, "http", 4))
            {
                //printf("href: %s\n", href);
                //handling logic for what to do with link found
                pthread_mutex_lock(&q_mutex);
                enqueue(frontier, (char *)href);
                pthread_mutex_unlock(&q_mutex);
            }
            xmlFree(href);
        }
        xmlXPathFreeObject(result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    //printf("find done\n");
    return 0;
}

void *crawler(void *ignore)
{

    int *retVal = 0;
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;
    ENTRY hurl;
    char *url = NULL;
    curl_global_init(CURL_GLOBAL_DEFAULT);

    pthread_mutex_lock(&count_mutex);
    pthread_mutex_lock(&running_mutex);
    //pthread_mutex_lock(&q_mutex);

    while ((!empty(frontier) || anyone_running) && png_count < num_pngs)
    {
        //pthread_mutex_unlock(&q_mutex);
        pthread_mutex_unlock(&count_mutex);
        pthread_mutex_unlock(&running_mutex);

        pthread_mutex_lock(&q_mutex);
        url = dequeue(frontier);
        //printf("url: %s\n", url);
        pthread_mutex_unlock(&q_mutex);

        //if url is not NULL
        if (url == NULL){
            //wait before checking the queue again
            nanosleep(&wait_time, NULL);
        }
        else
        {
            //printf("url: %s\n", url);
            hurl.key = url;
            //hurl.data = newUrl;
            // check if url is in ht
            pthread_mutex_lock(&ht_mutex);
            if (hsearch(hurl, FIND) == NULL)
            {
                //add to ht
                hsearch(hurl, ENTER);
                pthread_mutex_unlock(&ht_mutex);
                visited_array[link_count++] = url;
                //check_link(url, curl_handle);

                pthread_mutex_lock(&running_mutex);
                anyone_running += 1;
                pthread_mutex_unlock(&running_mutex);
                //printf("");
                //printf("checking %s\n", url);
                // curl call to url

                curl_handle = easy_handle_init(&recv_buf, url);
                if (curl_handle == NULL)
                {
                    fprintf(stderr, "Curl initialization failed. Exiting...\n");
                    curl_global_cleanup();
                    abort();
                }

                //printf("url: %s\n", url);
                res = curl_easy_perform(curl_handle);

                if (res != CURLE_OK)
                {
                    if (strcmp(logfile, ""))
                    {
                        //write to logfile;
                        pthread_mutex_lock(&log_mutex);
                        FILE *log;
                        log = fopen(logfile, "a");
                        fputs(url, log);
                        fputs("\n", log);
                        fclose(log);
                        pthread_mutex_unlock(&log_mutex);
                    }
                }
                else
                {
                    //printf("checking: %s\n", url);
                    check_link(curl_handle, &recv_buf);
                }
                free(url);
                pthread_mutex_lock(&running_mutex);
                anyone_running -= 1;
                pthread_mutex_unlock(&running_mutex);
            }
            else{
                pthread_mutex_unlock(&ht_mutex);
            }
            //printf("empty: %d\n", empty(frontier));
        }
        pthread_mutex_lock(&running_mutex);
        pthread_mutex_lock(&count_mutex);
        //pthread_mutex_lock(&q_mutex);
    }
    pthread_mutex_unlock(&count_mutex);
    pthread_mutex_unlock(&running_mutex);
    //pthread_mutex_unlock(&q_mutex);

    // exit conditions:
    // All threads are done and frontier empty
    // pngs full

    // repeat

    // on exit, cleanup
    cleanup(curl_handle, &recv_buf);
    pthread_exit(retVal);
}

int main(int argc, char **argv)
{
    struct timeval begin, end;
    gettimeofday(&begin, NULL);

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
    char *seedurl = argv[argc - 1];

    //frontier for pages to visit
    frontier = malloc(sizeof(frontier->urls)+sizeof(int)*3);
    frontier->front = 0;
    frontier->rear = 0;
    frontier->count = 0;

    //htable for urls_visited
    //visited = calloc(1, sizeof(visited));
    hcreate(1000);

    wait_time.tv_sec = 0;
    wait_time.tv_nsec = 5000 * num_threads;
    //initialize mutex
    pthread_mutex_init(&q_mutex, NULL);
    pthread_mutex_init(&count_mutex, NULL);
    pthread_mutex_init(&ht_mutex, NULL);
    pthread_mutex_init(&log_mutex, NULL);
    pthread_mutex_init(&png_mutex, NULL);
    pthread_mutex_init(&running_mutex, NULL);
    pthread_mutex_init(&link_mutex, NULL);

    enqueue(frontier, seedurl);

    //clean up files for use
    if (strcmp(logfile, ""))
    {
        FILE *file;
        file = fopen(logfile, "wb");
        fclose(file);
    }
    FILE *file;
    file = fopen("png_urls.txt", "wb");
    fclose(file);

    //create threads

    pthread_t crawlers[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        pthread_create(&crawlers[i], NULL, crawler, NULL);
        //printf("created thread %d\n", i);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(crawlers[i], NULL);
        //printf("joined thread %d\n", i);
    }

    // printf("yes\n");
    // for(int i = 0; i < frontier->rear; i++){
    //     char* printurl = dequeue(frontier);
    //     printf("%s\n", printurl);
    //     free(printurl);
    // }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - begin.tv_sec) + ((end.tv_usec - begin.tv_usec) / 1000000.0);
    printf("paster2 execution time: %6lf seconds\n", elapsed);

    //char* printurl = dequeue(frontier);
    //printf("link = %s\n", printurl);

    //cleanup
    pthread_mutex_destroy(&q_mutex);
    pthread_mutex_destroy(&count_mutex);
    pthread_mutex_destroy(&ht_mutex);
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&png_mutex);
    pthread_mutex_destroy(&running_mutex);
    pthread_mutex_destroy(&link_mutex);

    //curl_global_cleanup();
    for (int i = frontier->front; i <= frontier->rear; i++){
        free(frontier->urls[i]);
    }
    hdestroy();
    return 0;
}