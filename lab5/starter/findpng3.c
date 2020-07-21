#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <curl/multi.h>
#include <time.h>
#include <search.h> /* for hcreate */

#include "cURL/curl_util.h"

#define MAX_WAIT_MSECS 30 * 1000 /* Wait max. 30 seconds */
#define CT_PNG "image/png"
#define CT_HTML "text/html"
typedef unsigned char U8;
#define BUF_SIZE 1048576 /* 1024*1024 = 1M */

//data structure for frontier
typedef struct char_queue
{
    int front;
    int rear;
    int count;
    char *urls[1000];
} * char_queue_p;

//functions for queue
int empty(char_queue_p queue)
{
    return queue->front == queue->rear;
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
        free(queue->urls[queue->front]);
        queue->front++;
        queue->count--;
    }
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

//global variables
int png_count = 0;
int num_pngs = 50;
char logfile[100] = "";
char_queue_p frontier;

//additional functions
int find_http(char *buf, int size, int follow_relative_links, const char *base_url, CURL *curl_handle);
int check_link(CURL *curl_handle, RECV_BUF *p_recv_buf);
htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset(xmlDocPtr doc, xmlChar *xpath);
//check is signature matches png
int is_png(U8 *buf)
{
    U8 signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    //U8 header[8];
    int equal = 1;
    for (int i = 0; i < 8; i++)
    {
        if (buf[i] != signature[i])
        {
            equal = 0;
        }
    }
    return equal;
}

//checks url for more links
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
                enqueue(frontier, (char *)href);
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
    printf("check: %s\n", checkurl);

    if (strcmp(logfile, ""))
    {
        //write to logfile;
        FILE *log;
        log = fopen(logfile, "a");
        fputs(checkurl, log);
        fputs("\n", log);
        fclose(log);
    }
    //printf("link: %s, type: %s\n", checkurl, ct);
    //printf("%s\n", p_recv_buf->buf);
    //printf("%lu\n", p_recv_buf->size);
    if (strstr(ct, CT_HTML))
    {
        find_http(p_recv_buf->buf, p_recv_buf->size, 1, checkurl, curl_handle);
    }
    else if (strstr(ct, CT_PNG))
    {
        U8 *header;
        memcpy(&header, p_recv_buf, sizeof(header));
        if (is_png(header))
        {
            //printf("png yes \n");
            //write to png_urls.txt;

            if (png_count < num_pngs)
            {
                png_count++;
                FILE *png_file;
                png_file = fopen("png_urls.txt", "a");
                fputs(checkurl, png_file);
                fputs("\n", png_file);
                fclose(png_file);
            }
        }
    }
    //printf("done check\n");
    return 0;
}

#define CNT 4

static void init(CURLM *cm, char *url, RECV_BUF *p_recv_buf, int index)
{
    CURL *eh = easy_handle_init(p_recv_buf, url);
    //CURL *eh = curl_easy_init();
    // curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
    // curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
    // curl_easy_setopt(eh, CURLOPT_URL, url);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, index);
    curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);
    curl_multi_add_handle(cm, eh);
}

int find_empty_index(RECV_BUF *recv_array[], int num_connections)
{
    for (int i = 0; i < num_connections; i++)
    {
        if (recv_array[i]->size == 0)
        {
            return i;
        }
    }
    return -1;
}

int main(int argc, char **argv)
{
    int c;
    int num_connections = 1;

    /* Maybe replace this with a call to main_getopt.c */
    while ((c = getopt(argc, argv, "t:m:v:")) != -1)
    {
        switch (c)
        {
        case 't':
            num_connections = strtoul(optarg, NULL, 10);
            if (num_connections <= 0)
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

    //htable
    hcreate(2000);
    //allocate memory for frontier
    frontier = malloc(sizeof(frontier->urls));
    frontier->front = 0;
    frontier->rear = 0;
    frontier->count = 0;

    ENTRY hurl;
    RECV_BUF *recv_buf[num_connections];
    for (int i = 0; i < num_connections; i++)
    {
        recv_buf_init(&recv_buf[i], BUF_SIZE);
        free(recv_buf[i]->buf);
    }
    //recv_buf_init(&recv_buf, BUF_SIZE);
    int concount = 0, index = 0;
    CURLM *cm = NULL;
    CURL *eh = NULL;
    CURLMsg *msg = NULL;
    CURLcode return_code = 0;
    int still_running = 0;
    int msgs_left = 0;
    int http_status_code;
    int find_index = 0;
    //const char *szUrl;

    curl_global_init(CURL_GLOBAL_ALL);

    cm = curl_multi_init();

    //initialize the curl for seedurl
    index = find_empty_index(recv_buf, num_connections);
    //printf("%d\n", index);
    init(cm, seedurl, recv_buf[index], index);
    concount++;
    hurl.key = seedurl;
    hsearch(hurl, ENTER);
    if (strcmp(logfile, ""))
    {
        //write to logfile;
        FILE *log;
        log = fopen(logfile, "a");
        fputs(seedurl, log);
        fputs("\n", log);
        fclose(log);
    }

    do
    {
        int numfds = 0;
        int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
        if (res != CURLM_OK)
        {
            fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
            return EXIT_FAILURE;
        }
        /*
         if(!numfds) {
            fprintf(stderr, "error: curl_multi_wait() numfds=%d\n", numfds);
            return EXIT_FAILURE;
         }
        */
        //printf("a");

        curl_multi_perform(cm, &still_running);
        while ((msg = curl_multi_info_read(cm, &msgs_left)))
        {
            //printf("in while\n");
            //printf("msg left = %d\n", msgs_left);
            if (msg->msg == CURLMSG_DONE)
            {
                eh = msg->easy_handle;
                //getting recv_buf using eh
                //curl_easy_perform(eh);
                //size_t bytes_read = 0;
                //curl_easy_recv(eh, &recv_buf.buf, BUF_SIZE, &recv_buf.size);
                //printf("%s\n", recv_buf.buf);
                return_code = msg->data.result;

                if (return_code != CURLE_OK)
                {

                    //fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                    //continue;
                }
                //if webpage doesnt error out
                else
                {
                    // Get HTTP status code
                    http_status_code = 0;
                    //szUrl = NULL;

                    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
                    curl_easy_getinfo(eh, CURLINFO_PRIVATE, &find_index);

                    if (http_status_code < 400)
                    {
                        //printf("checking\n");
                        check_link(eh, recv_buf[find_index]);
                        //printf("done check\n");
                    }
                }
                //remove curl from multi handle
                recv_buf[find_index]->size = 0;
                //free(recv_buf[find_index]->buf);
                //free(recv_buf[find_index]);
                curl_multi_remove_handle(cm, eh);
                curl_easy_cleanup(eh);
                concount--;
                //printf("%d\n", concount);
                //add new curls to multi handle
                //printf("empty = %d\n", empty(frontier));
                while (concount < num_connections && !empty(frontier))
                {
                    // /printf("in while\n");
                    //printf("count = %d\n", frontier->count);
                    char *newUrl = dequeue(frontier);
                    //printf("%s\n", newUrl);
                    //not in ht = not searched yet, so add to multi curl
                    hurl.key = newUrl;
                    if (hsearch(hurl, FIND) == NULL)
                    {
                        //add to ht
                        hsearch(hurl, ENTER);

                        //add new curl with new url
                        index = find_empty_index(recv_buf, num_connections);
                        printf("index = %d\n", index);
                        init(cm, newUrl, recv_buf[index], index);
                        concount++;
                    }
                    curl_multi_perform(cm, &still_running);
                }
                //curl_multi_perform(cm, &still_running);
            }
        }
        //printf("still running = %d\n", still_running);
    } while ((still_running || !empty(frontier)) && png_count < num_pngs);
    for (int i = 0; i < num_connections; i++)
    {
        if (recv_buf[i]->buf)
        {
            recv_buf_cleanup(recv_buf[i]);
        }
    }
    for (int i = frontier->front; i <= frontier->rear; i++)
    {
        free(frontier->urls[i]);
    }
    curl_multi_cleanup(cm);
    curl_global_cleanup();
    printf("done\n");
    return EXIT_SUCCESS;
}