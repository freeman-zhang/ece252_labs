#include <stdio.h>  /* printf needs to include this header file */
#include <stdlib.h> /* for malloc()                 */
#include <unistd.h> /* for getopt                   */
#include <getopt.h> /* to get rid of optarg linting */
#include <string.h>
#include <pthread.h>          /* for pthread                  */
#include <search.h>           /* for hcreate */
#include "png_util/lab_png.h" /* simple PNG data structures   */
#include "cURL/curl_util.h"   /* for header and write cb      */
#include <semaphore.h>

#define CT_PNG "image/png"
#define CT_HTML "text/html"
#define VISITED_HT_SIZE 100


//global vars
char_queue_p frontier;
struct hsearch_data *visited;
int png_count = 0;
int num_pngs = 50;
char logfile[100] = "";

//function declarations
int find_http(RECV_BUF *p_recv_buf, int follow_relative_links, const char *base_url, CURL *curl_handle);
int check_link(char* url, CURL *curl_handle, RECV_BUF *p_recv_buf);


int empty(char_queue_p queue)
{
    return queue->count == 0;
}

char* dequeue(char_queue_p queue)
{
    char *returl;
    if (!empty(queue))
    {
        memcpy(&returl, &queue->urls[queue->front], sizeof(queue->urls[queue->front]));
        //free(queue->urls[queue->front]);
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

    memcpy(&queue->urls[queue->rear], &url, sizeof(url));
    //strcpy(queue->urls[queue->rear], url);
    queue->count++;
    return 0;
}

int check_link(char* url, CURL *curl_handle, RECV_BUF *p_recv_buf){
    CURLcode res;
    char *ct = NULL;
    
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if (res != CURLE_OK || ct == NULL)
    {
        fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }
    
    //printf("link: %s, type: %s\n", url, ct);
    ENTRY hurl, *hret;
    hurl.key = url;
    hurl.data = url;
    if (strstr(ct, CT_HTML))
    {
        hsearch_r(hurl, ENTER, &hret, visited);
        find_http(p_recv_buf, 1, url, curl_handle);
        if (strcmp(logfile, ""))
        {
            //write to logfile;
            FILE *log;
            log = fopen(logfile, "a");
            fputs(url, log);
            fputs("\n", log);
            fclose(log);
        }
    }
    else if (strstr(ct, CT_PNG))
    {
        //printf("png yes \n");
        //add to ht
        hsearch_r(hurl, ENTER, &hret, visited);
        png_count++;

        //write to png_urls.txt;
        FILE *png_file;
        png_file = fopen("png_urls.txt", "a");
        fputs(url, png_file);
        fputs("\n", png_file);
        fclose(png_file);
  

    }
    return 0;
}


int find_http(RECV_BUF *p_recv_buf, int follow_relative_links, const char *base_url, CURL *curl_handle)
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar *)"//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;

    if (p_recv_buf->buf == NULL)
    {
        return 1;
    }

    doc = mem_getdoc(p_recv_buf->buf, p_recv_buf->size, base_url);
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
                char * url = (char*)href;
                ENTRY hurl, *hret;
                hurl.key = url;
                hurl.data = url;
                // check if url is in ht
                if (!hsearch_r(hurl, FIND, &hret, visited))
                {
                    //add to ht
                    enqueue(frontier, url);
                    hsearch_r(hurl, ENTER, &hret, visited);
                    //check_link(url, curl_handle);
             
                }
            }
            //xmlFree(href);
        }
        xmlXPathFreeObject(result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}


void *crawler(void *ignore)
{
    
    while (!empty(frontier) && png_count < num_pngs){
        
        CURL *curl_handle;
        CURLcode res;
        RECV_BUF recv_buf;
        curl_global_init(CURL_GLOBAL_DEFAULT);
        
        // dq a url from frontier
        //printf("%d\n", empty(frontier));
        char *url = dequeue(frontier); 
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
                FILE *log;
                log = fopen(logfile, "a");
                fputs(url, log);
                fputs("\n", log);
                fclose(log);

                //add to ht
                ENTRY hurl, *hret;
                hurl.key = url;
                hurl.data = url;
                hsearch_r(hurl, ENTER, &hret, visited);
            }
        }
        else{
        printf("checking: %s\n", url);
        check_link(url, curl_handle, &recv_buf); 
        }
        cleanup(curl_handle, &recv_buf);
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

    enqueue(frontier, seedurl);
    
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

    // printf("yes\n");
    // for(int i = 0; i < frontier->rear; i++){
    //     char* printurl = dequeue(frontier); 
    //     printf("%s\n", printurl);
    //     free(printurl);
    // }

    //char* printurl = dequeue(frontier); 
    //printf("link = %s\n", printurl);

    free(frontier);
    return 0;
}