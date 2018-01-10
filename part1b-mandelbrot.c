#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include "Mandel.h"
#include "draw.h"

//file name-part1b-mandelbrot.c
//author name- Fawad Masood Desmukh
//author ID - 3035294478
//date - 30th October 2017
//compilation command - gcc part1b-mandelbrot.c -o 1bmandel -l SDL2 -l m
//SDL2 library dependency

//global variables to use in signal handlers
int num_of_tasks_cmp=0;
int sigRecieved=0;

typedef struct message{
    int row_index;
    pid_t child_pid;
    float rowdata[IMAGE_WIDTH];
} MSG;

typedef struct task {
    int start_row;
    int num_of_rows;
} TASK;

void sigusr1_handler(int signum){
    sigRecieved=1;
}

void sigint_handler(int signum, siginfo_t *sig, void *v){
    printf("Process %d is interrupted by ^C. Bye Bye\n", (int) getpid());
    exit(num_of_tasks_cmp);
}

int main(int argc, char *argv[]){
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    signal(SIGUSR1, sigusr1_handler); //install sigUsr handler

    int task_pfd[2];//create task pipe
    pipe(task_pfd);

    int data_pfd[2];//create data pipe
    pipe(data_pfd);

    int numOfProcess = atoi(argv[1]);
    int numOfRowInTASK = atoi(argv[2]);

    float * pixels;
    pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
    if (pixels == NULL) {
  		printf("Out of memory-1!!\n");
  		exit(1);
    }

    int x,y;
    pid_t pid;
    int i;

    pid_t child_pid_arr[numOfProcess];

    //create child processes and store their pids
    for (i=0; i<numOfProcess;i++){
        pid = fork();
        if (pid==0){
            break;
        }else if (pid>0){
          int child_pid_created = pid;
          child_pid_arr[i]=child_pid_created;
        }
    }

    if(pid>0){
      //parent
      printf("Start collecting the image lines\n");

      close(data_pfd[1]);
      close(task_pfd[0]);

      int row_to_start_from;
      TASK write_task;

      //first distribution of tasks
      for (int n =0; n < numOfProcess;n++){
        write_task.num_of_rows=numOfRowInTASK;
        write_task.start_row = n*write_task.num_of_rows;
        write(task_pfd[1], &write_task,sizeof (write_task));
        kill(child_pid_arr[n],SIGUSR1);
        row_to_start_from= write_task.start_row+write_task.num_of_rows;
      }

      int readnum=0;

      //read result from children
      MSG msgReceived;
      while(read(data_pfd[0],&msgReceived,sizeof (msgReceived))>0){
        readnum++;
        int a;
        int rowNumber = msgReceived.row_index;
        for (a=0; a<IMAGE_HEIGHT; a++) {
          pixels[rowNumber*IMAGE_WIDTH+a]=msgReceived.rowdata[a];
        }
        int child_pid_rcvd = msgReceived.child_pid;

        //give task to child if not finished
        if(child_pid_rcvd!=0){
          write_task.num_of_rows=numOfRowInTASK;
          write_task.start_row=row_to_start_from;
          row_to_start_from=write_task.start_row+write_task.num_of_rows;
          if(readnum<IMAGE_HEIGHT){
            write(task_pfd[1], &write_task,sizeof (write_task));
            kill(child_pid_rcvd,SIGUSR1);
          }
          // if(row_to_start_from>800){
          if(readnum==IMAGE_HEIGHT){
            // printf("%d\n", readnum );
            break;
          }
        }
      }

      int status[numOfProcess];

      //kill and wait for children to terminate and store exit status
      for (int n=0;n<numOfProcess;n++){
        kill(child_pid_arr[n],SIGINT);
        waitpid(child_pid_arr[n],&status[n],0);
      }

      //get termination status
      for (int n=0;n<numOfProcess;n++){
        int num_of_tasks_completed = WEXITSTATUS(status[n]);
        printf("Child process %d terminated and completed %d tasks\n", child_pid_arr[n], num_of_tasks_completed  );
      }

      clock_gettime(CLOCK_MONOTONIC, &end_time);
      float difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
      printf("Total elapse time measured by the parent process = %.3f ms\n", difftime);

      struct rusage pUsage;
      getrusage(RUSAGE_SELF,&pUsage);

      struct rusage cUsage;
      getrusage(RUSAGE_CHILDREN,&cUsage);

      printf("Total time spent by all child processes in user mode = %.3f ms\n", cUsage.ru_utime.tv_sec*1000.0 + cUsage.ru_utime.tv_usec/1000.0);
      printf("Total time spent by all child processes in system mode = %.3f ms\n", cUsage.ru_stime.tv_sec*1000.0 + cUsage.ru_stime.tv_usec/1000.0);

      printf("Total time spent by parent process in user mode = %.3f ms\n", pUsage.ru_utime.tv_sec*1000.0 + pUsage.ru_utime.tv_usec/1000.0);
      printf("Total time spent by parent process in system mode = %.3f ms\n", pUsage.ru_stime.tv_sec*1000.0 + pUsage.ru_stime.tv_usec/1000.0);

      //Draw Image
      printf("Draw the image\n");
      DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);

    } else if (pid ==0){
      //child
      close(task_pfd[1]);
      close(data_pfd[0]);

      //set signal handler
      // signal(SIGINT,sigint_handler);
      struct sigaction sa;
      sigaction(SIGINT, NULL,&sa);
      sa.sa_flags=SA_SIGINFO;
      sa.sa_sigaction=sigint_handler;
      sigaction(SIGINT,&sa,NULL);

      printf("Child(%d): Start up. Wait for task!\n",(int) getpid());

      while(1){
          while(sigRecieved==0){
            pause();//wait for SIGUSR1 or SIGINT
          }

          sigRecieved=0;

          num_of_tasks_cmp++;

          struct timespec start_compute, end_compute;
          clock_gettime(CLOCK_MONOTONIC, &start_compute);

          printf("Child(%d): Start the computation ...\n",(int) getpid());

          TASK task;
          MSG msg;
          read(task_pfd[0],&task,sizeof(task));//get task from task pipe

          // printf("%d\n",task.start_row );

          float result[IMAGE_WIDTH];

          //calculate Mandelbrot result
          for (y=task.start_row; y<(task.start_row+task.num_of_rows)&&y<IMAGE_HEIGHT; y++){
            for (x=0; x<IMAGE_WIDTH;x++){
              result[x]=Mandelbrot(x,y);
            }
            memcpy(msg.rowdata,result, sizeof result);
            msg.row_index=y;
            if(y==(task.start_row+task.num_of_rows-1)){
              msg.child_pid=(int) getpid();
            }else if(y==(IMAGE_HEIGHT-1)){
              msg.child_pid=(int) getpid();
            }else{
              msg.child_pid=0;
            }
            //write result to data pipe
            write(data_pfd[1], &msg,sizeof (msg));
            memset(result,0,sizeof(result));
          }

          clock_gettime(CLOCK_MONOTONIC, &end_compute);
          float difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
          printf(" ... completed. Elapse time = %.3f ms\n", difftime);

        }
      }

      return 0;

}
