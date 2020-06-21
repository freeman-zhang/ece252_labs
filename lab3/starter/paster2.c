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

int empty(png_queue_p queue){
    return queue->count == 0;
}

int full(png_queue_p queue){
    return queue->count == queue->max;
}

//need sem for this
int enqueue(png_queue_p queue, struct recv_buf buf){
    if(!full(queue)){
        if(queue->rear == queue->max - 1){
            queue->rear = -1;
        }
        queue->rear = queue->rear + 1;
        memcpy(queue->queue[queue->rear].entry, buf.buf, buf.max_size);
        queue->queue[queue->rear].number = buf.seq;
        // printf("inserting number %d into index %d\n", queue->queue[queue->rear].number, queue->rear);

        queue->count++;
    }
    else{
        return -1;
        printf("full\n");
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
		printf("empty\n");
	}
    return retVal;
}

void print_queue(png_queue_p queue){
    for(int i = 0; i < queue->max; i++){
        printf("%d ", queue->queue[i].number);
    }
    printf("\n");
}

int producer(png_queue_p queue, int *num_found, sem_t *counter_sem, sem_t *buffer_sem, sem_t *enqueue_sem, char * url){
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
        // printf("cunt licker\n");
        sem_wait(counter_sem);
		char newUrl[100];
		strcpy(newUrl, url);
		char num_found_char[3];
		sprintf(num_found_char, "%d", *num_found);
		strcat(newUrl, num_found_char);
		
		curl_easy_setopt(curl, CURLOPT_URL, newUrl);
		
        
        // printf("making png %d\n", *num_found);
        *num_found = *num_found + 1;
        
        recv_buf_init(&recv_buf, BUF_SIZE);
        response = curl_easy_perform(curl);
        sem_post(counter_sem);
        if(response == CURLE_OK){
            sem_wait(buffer_sem);
            // printf("%d\n", recv_buf.seq);
            sem_wait(enqueue_sem);


            enqueue(queue, recv_buf);
            // printf("curl from url %s\n", newUrl);
            // printf("%d\n", recv_buf.seq);


            sem_post(enqueue_sem);
            // printf("posting\n");
            sem_post(buffer_sem);
        }
        else{
            printf("uhoh\n");
            retVal = -1;
            error = 1;
        }	
        recv_buf_cleanup(&recv_buf);
        // printf("what\n");
        // for(int i = 0; i < 21; i++){
        //     printf("%x ", queue->queue[0].entry[i]);
        // }
        // printf("\n");
        
    }
    // printf("what\n");
	curl_easy_cleanup(curl);
    curl_global_cleanup();
    // printf("%x\n", queue->queue[0].entry[0]);
	printf("Kms\n");
	//raise (SIGTSTP);
    return retVal;
}

int consumer(int shmid, sem_t *dequeue_sem, png_queue_p queue){
    printf("consuming...\n");
	shared_mem_p mem = shmat(shmid, NULL, 0);
	while (mem->num_inflated < 50){
		sem_wait(dequeue_sem);
		queue_entry_p strip = dequeue(queue);
		sem_post(dequeue_sem);
		if (strip != NULL){
			simple_PNG_p newpng = createPNG(strip->entry);
			//mem->num_inflated += 1;
			if (!inflateStrips(newpng, mem->final_buffer, &mem->offset)){
				printf("inflated %d\n", strip->number);
				mem->num_inflated += 1;
			}
		}
	}
	printf("consume done\n");
	shmdt(mem);
	//raise (SIGTSTP);
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

    int counter_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int queue_struct_shmid = shmget(IPC_PRIVATE, sizeof(struct png_queue), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int queue_shmid = shmget(IPC_PRIVATE, buffer_size * sizeof(struct queue_entry), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	int shmid = shmget(IPC_PRIVATE, buffer_size * sizeof(struct shared_mem), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( queue_struct_shmid == -1){
        perror("shmget");
    }

    //initilazing shared memory
    png_queue_p mem = shmat(queue_struct_shmid, NULL, 0);
    queue_entry_p entries = shmat(queue_shmid, NULL, 0);
    int *num_found = shmat(counter_shmid, NULL, 0);
	shared_mem_p shared = shmat(shmid, NULL, 0);
    *num_found = 0;
    printf("found you bitch\n");
    // mem->wait_time = sleep_time;
    // mem->queue;
    // mem->queue = malloc(sizeof(struct png_queue) * buffer_size);
    mem->queue = entries;
    mem->count = 0;
    mem->front = 0;
    mem->rear = -1;
    mem->max = buffer_size;
	shared->num_inflated = 0;
    // printf("%d\n", mem->num_found);

    sem_t *counter_sem = sem_open("counter", O_CREAT, 0644, 1);
    sem_t *buffer_sem = sem_open("buffer", O_CREAT, 0644, buffer_size);
    sem_t *enqueue_sem = sem_open("enqueue", O_CREAT, 0644, 1);
	sem_t *dequeue_sem = sem_open("dequeue", O_CREAT, 0644, 1);
    sem_init(counter_sem, 0, 1);
    sem_init(buffer_sem, 0, buffer_size);
    sem_init(enqueue_sem, 0, 1);
	sem_init(dequeue_sem, 0, 1);

    pid_t prod_pid[num_producers];
    pid_t con_pid[num_consumers];

    for(int i = 0; i < num_producers; i++){
        prod_pid[i] = fork();
        if(prod_pid[i] == 0){
            producer(mem, num_found, counter_sem, buffer_sem, enqueue_sem, url[i % 3]);
            return 0;
        }
    }
    for(int i = 0; i < num_consumers; i++){
        con_pid[i] = fork();
        if(con_pid[i] == 0){
            consumer(shmid, dequeue_sem, mem);
            return 0;
        }
    }
    while(*num_found < NUM_PNGS || shared->num_inflated < 50){
        // printf("its now %d\n", mem->num_found);
        sleep(1);
    }
    //for(int i = 0; i < num_producers; i++){
    //    // printf("%d\n", prod_pid[i]);
    //    kill(prod_pid[i], SIGTERM);
    //}
    //for(int i = 0; i < num_consumers; i++){
    //    kill(con_pid[i], SIGTERM);
    //}

    printf("Children are done\n");
    // printf("%x\n", mem->pngs->queue[0]);
    for(int i = 0; i < buffer_size; i++){
        printf("%d ", mem->queue[i].number);
    }
    printf("\n");

    // simple_PNG_p pngs[50] = {NULL};
    // for(int i = 0; i < 50; i++){
    //     pngs[i] = createPNG(mem->queue[i].entry, 5);
    // }

    // if(catPNG(pngs, 50, final_height, final_width) != 0){
    //     printf("Error occured when concatenating PNGs.\n");
    //     return -1;
    // }

    // print_queue(mem->pngs);
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