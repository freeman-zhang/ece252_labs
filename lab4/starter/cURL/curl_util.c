#include <string.h>
#include <stdlib.h>

#include "curl_util.h"
#define BUF_SIZE 1048576 /* 1024*1024 = 1M */

#define ECE252_HEADER "X-Ece252-Fragment: "
#define max(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define BUF_INC 524288 /* 1024*512  = 0.5M */

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
        strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0)
    {

        /* extract img sequence number */
        p->seq = atoi(p_recv + strlen(ECE252_HEADER));
    }
    return realsize;
}

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size)
    { /* hope this rarely happens */
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL)
        {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;

    if (ptr == NULL)
    {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL)
    {
        return 2;
    }

    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1; /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL)
    {
        return 1;
    }

    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL)
    {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL)
    {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL)
    {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len)
    {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3;
    }
    return fclose(fp);
}

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if (ptr == NULL || url == NULL)
    {
        return NULL;
    }

    /* init user defined call back function buffer */
    if (recv_buf_init(ptr, BUF_SIZE) != 0)
    {
        return NULL;
    }
    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL)
    {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    /* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

void cleanup(CURL *curl, RECV_BUF *ptr)
{
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    recv_buf_cleanup(ptr);
}
