/*
Aleksas Murauskas 
260718389
ECSE 427
Assignment 2 Question 1
*/
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>


//Initialize all Semaphores
static sem_t rw_mutex;
static sem_t mutex;

//initialize function calls
void *read_func(void *);
void *write_func(void *);
//global value 
static int glob=0;
//Holds total time of all threads
static long totalWriteTime=0;
static long totalReadTime=0;
//Holds the minimum value 
static long minReadTime=100000000;
static long minWriteTime= 100000000;
//Fields to hold the average time 
static long aveReadTime=0;
static long aveWriteTime=0;
//Fields to hold the maximum time
static long maxReadTime=0;
static long maxWriteTime=0;
//Holds the amount of loops each thread operates for 
int read_loops=0;
int write_loops=0;
//the divisor is the total number of iterations of the thread's command 
static int read_divisor;
static int write_divisor;
//Is the reader busy?
int readCount=0;

int get_random_sleep(){ //method that returns a random time to sleep
	return ((rand()%100 +1)*1000);
}

//Tread that describes the writers process
void *write_func(void *arg) {
	int loc, w= 0;
	while(w<write_loops){
		clock_t writeStartTime =clock(); //begin keeping time
		sem_wait(&rw_mutex);
		clock_t writeEndTime = clock(); //stop keeping time
		clock_t elapsed_time = writeEndTime- writeStartTime; //calculating elapsed time
		long elapsed_microsec = elapsed_time*1000000/CLOCKS_PER_SEC; //converts elapsed time into microseconds 
		if(elapsed_microsec< minWriteTime){ //updates minWriteTime
			minWriteTime=elapsed_microsec;
		}
		if(elapsed_microsec > maxWriteTime){ //updates maxWriteTime
			maxWriteTime=elapsed_microsec;
		}
		totalWriteTime=totalWriteTime+elapsed_microsec; //update total time write has been waiting
		write_divisor++; //Updates total number of writes
		//Write Occurs 
		loc = glob;
    	loc= loc+10; //Write updates the global value 
   		glob = loc;
   		sem_post(&rw_mutex); //unlock reader writer 
   		w++; //Increment loop var
   		//usleep(get_random_sleep());
	}	
}

//Tread that describes the readers process
void *read_func(void *arg) {
	int r=0, loc; 
	while(r<read_loops){
		clock_t readStartTime =clock();  //begin keeping time
		sem_wait(&mutex); //Lock the file
		readCount++; //increment counter
		if(readCount==1){
			sem_wait(&rw_mutex); //lock for both reader and writer
		}
		sem_post(&mutex); //unlock the file
		clock_t readEndTime = clock(); //stop keeping time
		clock_t elapsed_time = readEndTime- readStartTime; //calculating elapsed time
		long elapsed_microsec = elapsed_time*1000000/CLOCKS_PER_SEC; //Calculate the time in microseconds
		if(elapsed_microsec< minReadTime){ //updates minReadTime
			minReadTime=elapsed_microsec;
		}
		if(elapsed_microsec > maxReadTime){ //updates maxReadTime
			maxReadTime=elapsed_microsec;
		}
		loc = glob;
    	loc= loc; //accesses glob and reads it  
   		glob = loc;
		printf("Start:%ld\n",elapsed_microsec); //Print the time that elapsed during this loop
		totalReadTime=totalReadTime+elapsed_microsec;  //update total time read has been waiting
		read_divisor++;
		sem_wait(&mutex);
		readCount--;
		if(readCount==0){// unlock the file
			sem_post(&rw_mutex);
		}
		sem_post(&mutex);
		r++; //Increment loop var
		//usleep(get_random_sleep());
	}
}

int main(int argc, char *argv[]) {
	if (argc!=3){                            // Checks if too few inputs are given 
        printf("ERROR: Incorrect number of arguments\n");
        exit(1);
    }
    //initialize the amount 
	int writer_num=10, read_num=500, error_val=0;

	int inputw= atoi(argv[1]); //Convert input 2 to integer
	int inputr= atoi(argv[2]); //Convert input 3 ti integer 
	write_loops=inputw; //30  set write_loops 
	read_loops=inputr; //60   set read_loops
	int ignore=0;
	//initialize semaphore
	if (sem_init(&mutex, 0, 1) == -1) {
    	printf("ERROR: init semaphore\n"); //Check for error
   		exit(1);
  	}
  	if (sem_init(&rw_mutex, 0, 1) == -1) {
    	printf("ERROR: init semaphore\n"); //Check for error
   		exit(1);
  	}
	//create thread arrays
	pthread_t write_arr[writer_num];
	pthread_t read_arr[read_num];
	//Create reader threads
	for(int x =0; x<read_num;x++){
		error_val = pthread_create(&read_arr[x], NULL, read_func, &ignore);
  		if (error_val != 0) { //Check for error
   			printf("ERROR: Creating reader threads failed\n");
    		exit(1);
  		}
	}
	//Create writer threads
	for(int y=0;y<writer_num;y++){
		error_val = pthread_create(&write_arr[y], NULL, write_func, &ignore);
  		if (error_val != 0) { //Check for error
   			printf("ERROR: Creating writer threads failed\n");
    		exit(1);
  		}
	}
	
	//Join Writer threads
	for(int y=0;y<writer_num;y++){
		error_val = pthread_join(write_arr[y], NULL);
  		if (error_val != 0) { //Check for error
    		printf("ERRROR: Joining writer threads failed\n");
    		exit(1);
  		}
	}
	//Join Reader threads 
	for(int x=0;x<read_num;x++){
		error_val = pthread_join(read_arr[x], NULL);
  		if (error_val != 0) { //Check for error
    		printf("ERRROR: Joining reader threads failed\n");
    		exit(1);
  		}
	}
	//Calculate Average Times
	aveWriteTime = totalWriteTime/write_divisor;
	aveReadTime = totalReadTime/read_divisor;
	//print results
	printf("Final Global variable value is: %d\n\n", glob);
	//print writing times
	printf("The following times are in Microseconds\n");
	printf("------Writing Times-------\n");
	printf("Total number of writes: %d\n",write_divisor);
	printf("Mininmum: %ld\n", minWriteTime);
	printf("Average: %ld\n", aveWriteTime);
	printf("Maximum: %ld\n\n", maxWriteTime);
	//print reading times
	printf("------Reading Times-------\n");
	printf("Total number of reads: %d\n",read_divisor);
	printf("Mininmum: %ld\n", minReadTime);
	printf("Average: %ld\n", aveReadTime);
	printf("Maximum: %ld\n\n", maxReadTime);
	//Complete
	exit(0);
}
