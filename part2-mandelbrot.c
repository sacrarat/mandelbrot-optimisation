#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <pthread.h>
#include "Mandel.h"
#include "draw.h"

//file name-part2-mandelbrot.c
//student name- Fawad Masood Desmukh
//student ID - 3035294478
//date - November 16th 2017
//compilation command - gcc part2-mandelbrot.c -o 2mandel -l SDL2 -l m -pthread
//SDL2 library dependency
//version: 1.0
//development platform: Course VM

//initialize pthread conditional variables and mutex lock
pthread_cond_t notFULL = PTHREAD_COND_INITIALIZER;
pthread_cond_t notEMPTY = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//variable to check if bufferfull or not
int count = 0;

//variable to signal worker termination
int exitSignal = 0;
int bufferSize;

//declare result array
float * pixels;

//task struct
typedef struct task {
    int start_row;
    int num_of_rows;
} TASK;

//declare bounded buffer and helper pointers
TASK * buffer;
int fill_ptr=0;
int use_ptr=0;
int numOfRowInTASK;

//to get a task from the bounded buffer
TASK get(){
  TASK task = buffer[use_ptr];
  use_ptr=(use_ptr+1) % bufferSize;
  count--;
  return task;
}

//to put a task into the bounded buffer
void put(int val){
  TASK write_task;
  write_task.num_of_rows=numOfRowInTASK;
  write_task.start_row = val;
  buffer[fill_ptr]=write_task;
  fill_ptr=(fill_ptr+1)%bufferSize;
  count++;
}

//consumer thread
void *worker (void *arg){
  printf("Worker(%d): Start up. Wait for task!\n", *((int *) arg));

  int* num_of_tasks_completed = (int *) malloc (sizeof(int));

  while(1){
    //acquire mutex lock
    pthread_mutex_lock(&lock);
    while (count==0){
      if(exitSignal==1){
        //if all tasks completed release mutex lock and exit thread
        pthread_mutex_unlock(&lock);
        pthread_exit((void *) num_of_tasks_completed);
      }
      //wait until buffer not empty
      pthread_cond_wait(&notEMPTY,&lock);
    }

    //timing start
    struct timespec start_compute, end_compute;
    clock_gettime(CLOCK_MONOTONIC, &start_compute);
    printf("Worker(%d): Start the computation ...\n",*((int *) arg));

    TASK task;
    //get a task from the buffer
    task = get();
    *num_of_tasks_completed=*num_of_tasks_completed+1;

    //signal buffer is not full
    pthread_cond_signal(&notFULL);
    //release mutex lock
    pthread_mutex_unlock(&lock);

    //compute mandelbrot result
    float result[IMAGE_WIDTH];
    int x,y;
    for (y=task.start_row; y<(task.start_row+task.num_of_rows)&&y<IMAGE_HEIGHT; y++){
      for (x=0; x<IMAGE_WIDTH;x++){
        result[x]=Mandelbrot(x,y);
      }

      int a;
      int rowNumber = y;
      for (a=0; a<IMAGE_HEIGHT; a++) {
        pixels[rowNumber*IMAGE_WIDTH+a]=result[a];
      }
    }

    //timing end
    clock_gettime(CLOCK_MONOTONIC, &end_compute);
    float difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
    printf("Worker(%d): ... completed. Elapse time = %.3f ms\n", *((int *) arg),difftime);
  }

}

int main(int argc, char *argv[]){
    //start process clock
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
    if (pixels == NULL) {
      printf("Out of memory-1!!\n");
      exit(1);
    }

    //initialize command line variables
    int numOfWorkers = atoi(argv[1]);
    numOfRowInTASK = atoi(argv[2]);
    bufferSize = atoi(argv[3]);

    //initialize thread handlers
    pthread_t workers[numOfWorkers];

    //store thread ids
    int thread_ids[numOfWorkers];
    int z;
    for (z=0; z<numOfWorkers;z++){
      thread_ids[z]=z;
    }
    //create and run worker threads
    for (z = 0; z < numOfWorkers; z++){
      pthread_create(&workers[z], NULL, worker, (void *) &thread_ids[z]);
    }

    //create bounded buffer
    buffer = (TASK *) malloc(bufferSize * sizeof(TASK));

    //initialize bounded buffer
    int i;
    int row_to_start_from;
    for (i=0; i< bufferSize; i++){
      put(i*numOfRowInTASK);
      row_to_start_from= i*numOfRowInTASK+numOfRowInTASK;
    }

    while (1){
        //acquire mutex lock
        pthread_mutex_lock(&lock);
        while (count==bufferSize){
          //wait until buffer not full
          pthread_cond_wait(&notFULL, &lock);
        }

        //put task into bounded buffer
        put(row_to_start_from);
        row_to_start_from=row_to_start_from+numOfRowInTASK;

        pthread_cond_signal(&notEMPTY);
        pthread_mutex_unlock(&lock);

        if (row_to_start_from>IMAGE_HEIGHT){//or ==?
          //signal workers if tasks finished
          exitSignal = 1;
          break;
        }
    }

    int * returnValue=0;
    for (int n =0; n<numOfWorkers;n++){
      //wait for threads to terminate
      pthread_join(workers[n],(void **) &returnValue);
      printf("Worker thread %d has terminated and completed %d tasks. \n", n, *returnValue );
    }

    printf("All worker threads have terminated\n");

    //get usage statistics
    struct rusage pUsage;
    getrusage(RUSAGE_SELF,&pUsage);

    printf("Total time spent by process and its threads in user mode = %.3f ms\n", pUsage.ru_utime.tv_sec*1000.0 + pUsage.ru_utime.tv_usec/1000.0);
    printf("Total time spent by  process and its threads in system mode = %.3f ms\n", pUsage.ru_stime.tv_sec*1000.0 + pUsage.ru_stime.tv_usec/1000.0);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    float difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
    printf("Total elapse time measured by the process = %.3f ms\n", difftime);

    printf("Draw the image\n");
    //create image
    DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);
}
