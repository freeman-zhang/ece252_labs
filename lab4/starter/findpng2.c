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
    char *urls[100];
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
//semaphores
sem_t eq_sem;
sem_t dq_sem;
sem_t count_sem;
sem_t ht_sem;
sem_t log_sem;
sem_t png_sem;
sem_t running_sem;

//function declarations
int find_http(char *buf, int size, int follow_relative_links, const char *base_url, CURL *curl_handle);
int check_link(char *url, CURL *curl_handle, RECV_BUF *p_recv_buf);
// int hcreate_r(size_t nel, struct hsearch_data *htab);
// int hsearch_r(ENTRY item, ACTION action, ENTRY **retval, struct hsearch_data *htab);
// void hdestroy_r(struct hsearch_data *htab);

int empty(char_queue_p queue)
{
    return queue->count == 0;
}

char *dequeue(char_queue_p queue)
{
    char *returl;
    if (!empty(queue))
    {
        memcpy(&returl, &queue->urls[queue->front], sizeof(queue->urls[queue->front]));
        //returl = strdup(queue->urls[queue->front]);
        //free(queue->urls[queue->front]);
        queue->front++;
        queue->count--;
    }
    else
    {
        return NULL;
    }
    //free(queue->urls[queue->front]);
    return returl;
}

int enqueue(char_queue_p queue, char *url)
{
    if (empty(queue))
    {
        queue->front = 0;
        queue->rear = 0;
    }
    else
    {
        queue->rear = queue->rear + 1;
    }

    memcpy(&queue->urls[queue->rear], &url, sizeof(url));
    //queue->urls[queue->rear] = strdup(url);
    //free(url);
    //strcpy(queue->urls[queue->rear], url);
    queue->count++;
    return 0;
}

int check_link(char *url, CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    char *ct = NULL;

    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if (res != CURLE_OK || ct == NULL)
    {
        fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    //printf("link: %s, type: %s\n", url, ct);
    ENTRY hurl;
    hurl.key = url;
    //hurl.data = url;
    if (strstr(ct, CT_HTML))
    {
        sem_wait(&ht_sem);
        hsearch(hurl, ENTER);
        visited_array[link_count++] = url;
        sem_post(&ht_sem);

        find_http(p_recv_buf->buf, p_recv_buf->size, 1, url, curl_handle);

        if (strcmp(logfile, ""))
        {
            //write to logfile;
            sem_wait(&log_sem);
            FILE *log;
            log = fopen(logfile, "a");
            fputs(url, log);
            fputs("\n", log);
            fclose(log);
            sem_post(&log_sem);
        }
    }
    else if (strstr(ct, CT_PNG))
    {
        //printf("png yes \n");
        //add to ht
        sem_wait(&ht_sem);
        hsearch(hurl, ENTER);
        visited_array[link_count++] = url;
        sem_post(&ht_sem);

        //write to png_urls.txt;
        sem_wait(&png_sem);
        sem_wait(&count_sem);
        if (png_count < num_pngs)
        {
            png_count++;
            sem_post(&count_sem);
            FILE *png_file;
            png_file = fopen("png_urls.txt", "a");
            fputs(url, png_file);
            fputs("\n", png_file);
            fclose(png_file);
        }
        else
        {
            sem_post(&count_sem);
        }
        sem_post(&png_sem);

        if (strcmp(logfile, ""))
        {
            //write to logfile;
            sem_wait(&log_sem);
            FILE *log;
            log = fopen(logfile, "a");
            fputs(url, log);
            fputs("\n", log);
            fclose(log);
            sem_post(&log_sem);
        }
    }
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
                char *newUrl = strdup((char *)href);
                ENTRY hurl;
                hurl.key = newUrl;
                //hurl.data = newUrl;
                // check if url is in ht
                sem_wait(&ht_sem);
                if (!hsearch(hurl, FIND))
                {
                    //add to ht
                    sem_wait(&eq_sem);
                    enqueue(frontier, newUrl);
                    sem_post(&eq_sem);
                    hsearch(hurl, ENTER);
                    visited_array[link_count++] = newUrl;
                    //check_link(url, curl_handle);
                }
                sem_post(&ht_sem);
            }
            xmlFree(href);
        }
        xmlXPathFreeObject(result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

void *crawler(void *ignore)
{

    int *retVal = 0;
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;
    curl_global_init(CURL_GLOBAL_DEFAULT);

    while ((!empty(frontier) || anyone_running) && png_count < num_pngs)
    {
        char *url;
        sem_post(&count_sem);
        sem_post(&running_sem);
        if (!empty(frontier))
        {

            url = dequeue(frontier);
            sem_post(&dq_sem);
            sem_post(&eq_sem);

            sem_wait(&running_sem);
            anyone_running += 1;
            sem_post(&running_sem);

            // dq a url from frontier
            //printf("%d\n", empty(frontier));

            printf("");
            //printf("checking %s\n", url);
            // curl call to url

            //if url is not NULL
            if (url)
            {
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
                        sem_wait(&log_sem);
                        FILE *log;
                        log = fopen(logfile, "a");
                        fputs(url, log);
                        fputs("\n", log);
                        fclose(log);
                        sem_post(&log_sem);
                    }
                    //add to ht
                    ENTRY hurl;
                    hurl.key = url;
                    //hurl.data = url;
                    sem_wait(&ht_sem);
                    hsearch(hurl, ENTER);
                    visited_array[link_count++] = url;
                    sem_post(&ht_sem);
                }
                else
                {
                    //printf("checking: %s\n", url);
                    check_link(url, curl_handle, &recv_buf);
                }
            }
            //free(url);
            sem_wait(&running_sem);
            anyone_running -= 1;
            sem_post(&running_sem);
        }
        else
        {
            sem_post(&dq_sem);
            sem_post(&eq_sem);
        }
        sem_wait(&running_sem);
        sem_wait(&eq_sem);
        sem_wait(&dq_sem);
        sem_wait(&count_sem);
    }
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
    char seedurl[256];
    strcpy(seedurl, argv[argc - 1]);

    //frontier for pages to visit
    frontier = malloc(sizeof(int) * 3 + sizeof(char) * 100 * 256);
    frontier->front = 0;
    frontier->rear = 0;
    frontier->count = 0;

    //htable for urls_visited
    //visited = calloc(1, sizeof(visited));
    hcreate(1000);

    //initialize sems
    sem_init(&eq_sem, 1, 0);
    sem_init(&dq_sem, 1, 0);
    sem_init(&count_sem, 1, 0);
    sem_init(&ht_sem, 1, 1);
    sem_init(&log_sem, 1, 1);
    sem_init(&png_sem, 1, 1);
    sem_init(&running_sem, 1, 0);

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
    sem_destroy(&eq_sem);
    sem_destroy(&dq_sem);
    sem_destroy(&count_sem);
    sem_destroy(&ht_sem);
    sem_destroy(&log_sem);
    sem_destroy(&png_sem);
    sem_destroy(&running_sem);

    //curl_global_cleanup();
    free(frontier);
    hdestroy();
    return 0;
}