#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                 */
#include <unistd.h>             /* for getopt                   */
#include <getopt.h>             /* to get rid of optarg linting */
#include <curl/curl.h>          /* for cURL                     */
#include <pthread.h>            /* for pthread                  */
#include <string.h>
#include <time.h>
#include <errno.h>
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
#define NUM_PNGS 50
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define SHM_SIZE 256 

typedef struct queue_entry{
    U8 entry[BUF_SIZE];
    int number;
} *queue_entry_p;

typedef struct png_queue{
    int front;
    int rear;
    int count;
    int max;
    struct queue_entry *queue;
} *png_queue_p;

typedef struct shared_mem{
    int wait_time;
    int offset;
    int num_found;
	int num_inflated;
	U8 final_buffer[final_height * (final_width * 4 + 1)];
    int head;
    struct png_queue pngs;
} *shared_mem_p;

int msleep(long tms){
    struct timespec tim;
    int ret;
 
    if (tms < 0)
    {
        errno = EINVAL;
        return -1;
    }
 
    tim.tv_sec = tms / 1000;
    tim.tv_nsec = (tms % 1000) * 1000000;
 
    do {
        ret = nanosleep(&tim, &tim);
    } while (ret && errno == EINTR);
 
    return ret;
}

int empty(png_queue_p queue){
    return queue->count == 0;
}

int full(png_queue_p queue){
    return queue->count == queue->max;
}

int enqueue(png_queue_p queue, struct recv_buf buf){
    if(!full(queue)){
        if(queue->rear == queue->max - 1){
            queue->rear = -1;
        }
        queue->rear = queue->rear + 1;
        memcpy(queue->queue[queue->rear].entry, buf.buf, buf.max_size);
        queue->queue[queue->rear].number = buf.seq;

        queue->count++;
    }
    else{
        return -1;
    }
    return 0;
}

queue_entry_p dequeue(png_queue_p queue){
	queue_entry_p retVal;
	if (!empty(queue)){
		retVal = &queue->queue[queue->front++];

		if(queue->front == queue->max){
			queue->front = 0;
		}
		queue->count--;
	}
	else{
		retVal = NULL;
	}
    return retVal;
}

void print_queue(png_queue_p queue){
    for(int i = 0; i < queue->max; i++){
        printf("%d ", queue->queue[i].number);
    }
    printf("\n");
}

int producer(png_queue_p queue, int *num_found, sem_t *counter_sem, sem_t *buffer_sem, sem_t *enqueue_sem, sem_t *recv_sem, char * url){
    int retVal = 0;
    volatile int error = 0;

    CURL *curl;
    CURLcode response;

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
    while(*num_found < NUM_PNGS && !error){
		char newUrl[100];
		strcpy(newUrl, url);
		char num_found_char[3];
		sem_wait(counter_sem);
		sprintf(num_found_char, "%d", *num_found);
		strcat(newUrl, num_found_char);
		*num_found = *num_found + 1;
		sem_post(counter_sem);
		
		curl_easy_setopt(curl, CURLOPT_URL, newUrl);

		sem_wait(recv_sem);

        recv_buf_init(&recv_buf, BUF_SIZE);
        response = curl_easy_perform(curl);
        sem_post(recv_sem);
        if(response == CURLE_OK){
            sem_wait(buffer_sem);
            sem_wait(enqueue_sem);

            enqueue(queue, recv_buf);
            sem_post(enqueue_sem);
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
    return retVal;
}

int consumer(shared_mem_p mem, sem_t *dequeue_sem, sem_t *buffer_sem, png_queue_p queue){
	while (mem->num_inflated < 50){
        msleep(mem->wait_time);
		sem_wait(dequeue_sem);
		queue_entry_p strip = dequeue(queue);
		sem_post(dequeue_sem);
		if (strip != NULL){
			simple_PNG_p newpng = createPNG(strip->entry);
			if (!inflateStrips(newpng, mem->final_buffer, &strip->number)){
				mem->num_inflated += 1;
			}
            sem_post(buffer_sem);
		}
	}
	shmdt(mem);
    return 0;
}

int main(int argc, char **argv){

    if(argc < 6){
        printf("Not enough args\n");
        return -1;
    }

    struct timeval begin, end;
    gettimeofday(&begin, NULL);
	
	int buffer_size = atoi(argv[1]);		//B
    int num_producers = atoi(argv[2]);		//P
	int num_consumers = atoi(argv[3]);		//C
	int sleep_time = atoi(argv[4]); 		//X
    int img_num = atoi(argv[5]);			//N
	
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

    int counter_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int queue_struct_shmid = shmget(IPC_PRIVATE, sizeof(struct png_queue), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int queue_shmid = shmget(IPC_PRIVATE, buffer_size * sizeof(struct queue_entry), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	int shmid = shmget(IPC_PRIVATE, buffer_size * sizeof(struct shared_mem), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( queue_struct_shmid == -1){
        perror("shmget");
    }

    png_queue_p mem = shmat(queue_struct_shmid, NULL, 0);
    queue_entry_p entries = shmat(queue_shmid, NULL, 0);
    int *num_found = shmat(counter_shmid, NULL, 0);
	shared_mem_p shared = shmat(shmid, NULL, 0);
    *num_found = 0;
    mem->queue = entries;
    mem->count = 0;
    mem->front = 0;
    mem->rear = -1;
    mem->max = buffer_size;
    shared->wait_time = sleep_time;
	shared->num_inflated = 0;

    sem_t *counter_sem = sem_open("counter", O_CREAT, 0644, 1);
    sem_t *buffer_sem = sem_open("buffer", O_CREAT, 0644, buffer_size);
    sem_t *enqueue_sem = sem_open("enqueue", O_CREAT, 0644, 1);
	sem_t *dequeue_sem = sem_open("dequeue", O_CREAT, 0644, 1);
	sem_t *recv_sem = sem_open("receive", O_CREAT, 0644, 1);
    sem_init(counter_sem, 1, 1);
    sem_init(buffer_sem, 1, buffer_size);
    sem_init(enqueue_sem, 1, 1);
	sem_init(dequeue_sem, 1, 1);
	sem_init(recv_sem, 1, 1);

    pid_t prod_pid[num_producers];
    pid_t con_pid[num_consumers];

    for(int i = 0; i < num_producers; i++){
        prod_pid[i] = fork();
        if(prod_pid[i] == 0){
            producer(mem, num_found, counter_sem, buffer_sem, enqueue_sem, recv_sem, url[i % 3]);
            return 0;
        }
    }
    for(int i = 0; i < num_consumers; i++){
        con_pid[i] = fork();
        if(con_pid[i] == 0){
            consumer(shared, dequeue_sem, buffer_sem, mem);
            return 0;
        }
    }
    while(*num_found < NUM_PNGS || shared->num_inflated < 50){
        sleep(1);
    }

    for(int i = 0; i < num_producers; i++){
        kill(prod_pid[i], SIGTERM);
    }
    for(int i = 0; i < num_consumers; i++){
        kill(con_pid[i], SIGTERM);
    }

	U8* png_buf = entries[0].entry;
	simple_PNG_p png = createPNG(png_buf);
	int final_offset = 50;
	catPNG(png, shared->final_buffer, final_height, final_width, &final_offset);

    gettimeofday(&end, NULL);
    long double elapsed = (end.tv_sec - begin.tv_sec) + ((end.tv_usec - begin.tv_usec)/1000000.0);
    printf("paster2 execution time: %6Lf seconds\n", elapsed);

    shmdt(mem);
    shmdt(entries);
    shmdt(num_found);
    shmdt(shared);
    return 0;
}