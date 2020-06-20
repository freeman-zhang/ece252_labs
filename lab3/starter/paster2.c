#include <stdio.h>	            /* printf needs to include this header file */
#include <stdlib.h>             /* for malloc()                 */
#include <unistd.h>             /* for getopt                   */
#include <getopt.h>             /* to get rid of optarg linting */
#include <curl/curl.h>          /* for cURL                     */
#include <pthread.h>            /* for pthread                  */
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

simple_PNG_p pngs[num_pngs] = {NULL};

struct shared_mem{
    sem_t counter;
    sem_t writer;
    sem_t checker;
    simple_PNG_p *pngs;
} *shared_mem_p;


int fetch_png(int counter_shmid, int buffer_shmid, sem_t *sem, void * url){
    int *num_found = shmat(counter_shmid, NULL, 0);
    // simple_PNG_p pngs = shmat(buffer_shmid, NULL, 0);

    int retVal = 0;
    int strip_num = 0;
    volatile int error = 0;

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    RECV_BUF recv_buf;

    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb_curl3);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_buf);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb_curl);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&recv_buf);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    }
    while(*num_found < 50 && !error){
        sem_wait(sem);
        strip_num = *num_found;
        sem_post(sem);
        recv_buf_init(&recv_buf, BUF_SIZE);
        res = curl_easy_perform(curl);
        if(res == CURLE_OK){
            if(!pngs[recv_buf.seq]){
                pngs[recv_buf.seq] = createPNG((U8*)recv_buf.buf, recv_buf.size);
                num_found++;
            }
        }
        else{
            retVal = -1;
            error = 1;
        }
		recv_buf_cleanup(&recv_buf);
    }
    shmdt(num_found);
	curl_easy_cleanup(curl);
    curl_global_cleanup();
    return;
}

int producer(int shmid, sem_t *sem, char *url){
    int *num_found = shmat(shmid, NULL, 0);
    while(*num_found < 50){
        sem_wait(sem);
        printf("woop %d\n", *num_found);
        sleep(1);
        *num_found = *num_found + 1;
        sem_post(sem);
    }
    //call fetch png
    shmdt(num_found);
    return;
}
int consumer(){
    printf("consuming...\n");
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
	
		
    char url[3][100];
	strcpy(url[0], "http://ece252-1.uwaterloo.ca:2530/image?img=");
	strcpy(url[1], "http://ece252-2.uwaterloo.ca:2530/image?img=");
	strcpy(url[2], "http://ece252-3.uwaterloo.ca:2530/image?img=");

    char img_num_char[2];
    sprintf(img_num_char, "%d", img_num);
	strcat(url[0], img_num_char);
	strcat(url[1], img_num_char);
	strcat(url[2], img_num_char);

    int buffer_shmid = shmget(IPC_PRIVATE, buffer_size * sizeof(struct simple_PNG), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int counter_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( buffer_shmid == -1 || counter_shmid == -1){
        perror("shmget");
    }

    int *num_found = shmat(counter_shmid, NULL, 0);
    *num_found = 0;
    simple_PNG_p pngs = shmat(buffer_shmid, NULL, 0);

    // printf("%d\n", *num_found);

    sem_t *counter_sem = sem_open("counter", O_CREAT, 0644, 1);
    sem_t *buffer_sem = sem_open("buffer", O_CREAT, 0644, 1);
    sem_init(counter_sem, 0, 1);
    sem_init(buffer_sem, 0, 1);

    pid_t prod_pid[num_producers];
    pid_t con_pid[num_consumers];

    for(int i = 0; i < num_producers; i++){
        prod_pid[i] = fork();
        if(prod_pid[i] == 0){
            producer(counter_shmid, counter_sem, url[i % 3]);
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
    while(*num_found < 50){
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