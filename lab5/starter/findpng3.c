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
    //printf("check: %s\n", url);
    if (strcmp(logfile, ""))
    {
        //write to logfile;
        FILE *log;
        log = fopen(logfile, "a");
        fputs(checkurl, log);
        fputs("\n", log);
        fclose(log);
    }

    //printf("link: %s, type: %s\n", url, ct);

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

static const char *urls[] = {
    "http://www.microsoft.com",
    "http://www.yahoo.com",
    "http://www.wikipedia.org",
    "http://slashdot.org"};
#define CNT 4

static size_t cb(char *d, size_t n, size_t l, void *p)
{
    /* take care of the data here, ignored in this example */
    (void)d;
    (void)p;
    return n * l;
}

static void init(CURLM *cm, int i)
{
    CURL *eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
    curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
    curl_easy_setopt(eh, CURLOPT_URL, urls[i]);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, urls[i]);
    curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);
    curl_multi_add_handle(cm, eh);
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

    CURLM *cm = NULL;
    CURL *eh = NULL;
    CURLMsg *msg = NULL;
    CURLcode return_code = 0;
    int still_running = 0, msgs_left = 0;
    int http_status_code;
    const char *szUrl;

    curl_global_init(CURL_GLOBAL_ALL);

    cm = curl_multi_init();

    for (int i = 0; i < CNT; ++i)
    {
        init(cm, i);
    }

    curl_multi_perform(cm, &still_running);

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
        curl_multi_perform(cm, &still_running);

        if ((msg = curl_multi_info_read(cm, &msgs_left)))
        {
            if (msg->msg == CURLMSG_DONE)
            {
                eh = msg->easy_handle;

                return_code = msg->data.result;
                if (return_code != CURLE_OK)
                {
                    fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                    continue;
                }

                // Get HTTP status code
                http_status_code = 0;
                szUrl = NULL;

                curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
                curl_easy_getinfo(eh, CURLINFO_PRIVATE, &szUrl);

                if (http_status_code == 200)
                {
                    printf("200 OK for %s\n", szUrl);
                }
                else
                {
                    fprintf(stderr, "GET of %s returned http status code %d\n", szUrl, http_status_code);
                }

                curl_multi_remove_handle(cm, eh);
                curl_easy_cleanup(eh);
            }
            else
            {
                fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
            }
        }
    } while (still_running);

    curl_multi_cleanup(cm);
    curl_global_cleanup();

    return EXIT_SUCCESS;
}
