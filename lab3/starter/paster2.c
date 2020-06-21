#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                 */
#include <unistd.h>             /* for getopt                   */
#include <getopt.h>             /* to get rid of optarg linting */
#include <curl/curl.h>          /* for cURL                     */
#include <pthread.h>            /* for pthread                  */
#include <string.h>
#include "png_util/lab_png.h"   /* simple PNG data structures   */
#include "cURL/curl_util.h"     /* for header and write cb      */
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>              /* For O_* constants */

#define final_height 300
#define final_width 400
#define num_pngs 50
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define SHM_SIZE 256 

// simple_PNG_p pngs[num_pngs] = {NULL};

typedef struct queue_entry{
    simple_PNG_p entry;
    int number;
} *queue_entry_p;

typedef struct png_queue{
    int front;
    int rear;
    int count;
    int max;
    queue_entry_p *queue;
} *png_queue_p;

typedef struct shared_mem{
    int wait_time;
    int offset;
    int num_found;
    int head;
    png_queue_p pngs;
} *shared_mem_p;

int empty(png_queue_p queue){
    return queue->count == 0;
}

int full(png_queue_p queue){
    return queue->count == queue->max;
}

int enqueue(png_queue_p queue, queue_entry_p png){
    if(!full(queue)){
        if(queue->rear == queue->max - 1){
            queue->rear = -1;
        }
        queue->queue[++queue->rear] = png;
        queue->count++;
    }
    else{
        // printf("full\n");
    }
    return 0;
}

queue_entry_p dequeue(png_queue_p queue){
    queue_entry_p retVal = queue->queue[queue->front++];

    if(queue->front == queue->max){
        queue->front = 0;
    }
    queue->count--;
    return retVal;
}

void print_queue(png_queue_p queue){
    for(int i = 0; i < queue->max; i++){
        printf("%d ", queue->queue[i]);
    }
    printf("\n");
}

int producer(int shmid, sem_t *counter_sem, sem_t *buffer_sem, char * url){
    shared_mem_p mem = shmat(shmid, NULL, 0);
	 
    int retVal = 0;
    volatile int error = 0;
    int split_number = 0;

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    RECV_BUF recv_buf;

    if(curl){
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb_curl3);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_buf);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb_curl);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&recv_buf);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		}
    while(mem->num_found < 50 && !error){
		char newUrl[100];
		strcpy(newUrl, url);
		char num_found_char[3];
		sprintf(num_found_char, "%d", mem->num_found);
		strcat(newUrl, num_found_char);
		
		curl_easy_setopt(curl, CURLOPT_URL, newUrl);
		
        sem_wait(counter_sem);
        // printf("making png %d\n", mem->num_found);
        split_number = mem->num_found;
        mem->num_found = mem->num_found + 1;
        sem_post(counter_sem);
        recv_buf_init(&recv_buf, BUF_SIZE);
        res = curl_easy_perform(curl);
        if(res == CURLE_OK){
            sem_wait(buffer_sem);
            simple_PNG_p res = createPNG((U8*)recv_buf.buf, recv_buf.size);
            queue_entry_p entry = malloc(sizeof(struct queue_entry));
            entry->entry = res;
            entry->number = split_number;
            enqueue(mem->pngs, entry);
            print_queue(mem->pngs);
            // printf("%s\n", newUrl);	
            // printf("making png %d\n", strip);
            //printf("curl from url %s\n", url);
            sem_post(buffer_sem);
        }
        else{
            printf("uhoh\n");
            retVal = -1;
            error = 1;
        }	
        recv_buf_cleanup(&recv_buf);
    }
	curl_easy_cleanup(curl);
    curl_global_cleanup();
    shmdt(mem);
    return retVal;
}

int consumer(){
    printf("consuming...\n");
    return 0;
}

//TODO load balancing
//TODO synchronization to avoid memory leaks from creating 2 same pngs
int main(int argc, char **argv){
	
	int buffer_size = atoi(argv[1]);		//B
    int num_producers = atoi(argv[2]);		//P
	int num_consumers = atoi(argv[3]);		//C
	int sleep_time = atoi(argv[4]); 		//X
    int img_num = atoi(argv[5]);			//N
		
    int thread_error = 0;
    thread_error++;
    sleep_time++;

	
    char url[3][100];
	strcpy(url[0], "http://ece252-1.uwaterloo.ca:2530/image?img=");
	strcpy(url[1], "http://ece252-2.uwaterloo.ca:2530/image?img=");
	strcpy(url[2], "http://ece252-3.uwaterloo.ca:2530/image?img=");

    char img_num_char[2];
    sprintf(img_num_char, "%d", img_num);
	strcat(url[0], img_num_char);
	strcat(url[1], img_num_char);
	strcat(url[2], img_num_char);

    strcat(url[0], "&part=");
	strcat(url[1], "&part=");
	strcat(url[2], "&part=");

    int shmid = shmget(IPC_PRIVATE, buffer_size * sizeof(struct shared_mem), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( shmid == -1){
        perror("shmget");
    }

    //initilazing shared memory
    shared_mem_p mem = shmat(shmid, NULL, 0);
    mem->num_found = 0;
    printf("found you bitch\n");
    mem->wait_time = sleep_time;
    mem->pngs = malloc(sizeof(struct png_queue));
    mem->pngs->queue = malloc(buffer_size * sizeof(struct queue_entry));
    mem->pngs->count = 0;
    mem->pngs->front = 0;
    mem->pngs->rear = -1;
    mem->pngs->max = buffer_size;

    // printf("%d\n", mem->num_found);

    sem_t *counter_sem = sem_open("counter", O_CREAT, 0644, 1);
    sem_t *buffer_sem = sem_open("buffer", O_CREAT, 0644, buffer_size);
    sem_init(counter_sem, 0, 1);
    sem_init(buffer_sem, 0, buffer_size);

    pid_t prod_pid[num_producers];
    pid_t con_pid[num_consumers];

    for(int i = 0; i < num_producers; i++){
        prod_pid[i] = fork();
        if(prod_pid[i] == 0){
            producer(shmid, counter_sem, buffer_sem, url[i % 3]);
            return 0;
        }
    }
    for(int i = 0; i < num_consumers; i++){
        con_pid[i] = fork();
        if(con_pid[i] == 0){
            consumer();
            return 0;
        }
    }
    while(mem->num_found < 50){
        // printf("its now %d\n", *num_found);
        sleep(1);
    }
    for(int i = 0; i < num_producers; i++){
        // printf("%d\n", prod_pid[i]);
        kill(prod_pid[i], SIGTERM);
    }
    for(int i = 0; i < num_consumers; i++){
        kill(con_pid[i], SIGTERM);
    }
    printf("Children are done\n");
    // for(int i = 0; i < buffer_size; i++){
    //     printf("%d\n", mem->pngs->queue[i]);
    // }
    print_queue(mem->pngs);
    //Delete shm
		
    // pthread_t threads[num_threads];
    // void *vr[num_threads];

    // for(int i = 0; i < num_threads; i++){
    //     pthread_create(&threads[i], NULL, run, url[i%3]);
    // }
    // for(int i = 0; i < num_threads; i++){
    //     pthread_join(threads[i], &vr[i]);
    //     if(*(int*)vr[i] == -1){
    //         printf("ERROR: thread %d failed, program will abort.\n", *(int*)vr[i]);
    //         thread_error = 1;
    //     }
    //     free(vr[i]);
    // }

    // if(thread_error){
    //     return -1;
    // }
	
	// if(catPNG(pngs, num_pngs, final_height, final_width) != 0){
    //     printf("Error occured when concatenating PNGs.\n");
    //     return -1;
    // }
	
	// for(int i = 0; i < num_pngs; i++){
    //     freePNG(pngs[i]);
    // }
    return 0;
}