/*
    Aleksas Murauskas
    260718389
    ECSE 427
    Assignment 1
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

void stpHandler(int s); //handles ^z
void intHandler(int s); //handles ^c
char *history[100]; //holds call history
int historyIndex = 0; //marks current position in the history
struct rlimit memlimiter; //Memory Limiter

void intHandler(int s){ //Signal handler for ^c
    printf("\nWould you like to exit the shell? [Y/N]:\n"); //Prompts user on the exit command 
}
void stpHandler(int s){ //Signal handler for ^z
    printf("\n ERROR: This Program does not allow the use of that command\n");
}

int recordHistory(char *line){ // This mwthod will record the history of the shell
    int end_line =strcasecmp(line, "\n"); //The program will not record \n in the history
	if(end_line == 0){
		return 0; 
	}
	if(historyIndex<100){ //As long as there is still space in the history 
	   history[historyIndex] = strdup(line); //adds the line to 
	   historyIndex++;// increment
	}
	return 0;
}

int reciteHistory(){ //This method will print the history 
    for(int z = 0; z < historyIndex; z++){ //Iterate through recorded history until historyIndex is reached S
            printf("   %u", (z+1));//Print the call number 
            printf("  %s \n", history[z]); //Print the line
        }
    return 0;
}
int check_exit(char *line){ //Checks if the entered tokens should have the program terminate
    int e= strcasecmp(line, "Y"); //Check for both upper and lowercase Y
    int e2= strcasecmp(line, "y");
    if(e==0||e2==0){
        exit(0); //Program Terminates
    }
    return 0;
}
int check_chdir(char *tokens[], int x){ //Checks change directory prompts 
    if(x < 2) //
    {
        printf("ERROR: Target Directory not visible\n"); //The directory was not specified
        return 1;
    }
    else if(x > 1) //Directory  is changed
    {
        tokens[0] = tokens[1];
        chdir(tokens[0]); //chdir is called
        return 1;
    }
    return 0;
}
int check_limit(char *tokens[],int x){ //checks for the limit keyword
    if(x < 2) //if limit alone is used, the program will print the current limit and max limit of the limiter
    {
        if (getrlimit(RLIMIT_DATA, &memlimiter) != 0) { 
            return 1;
        }
        printf("The current limit is: %lu \n", memlimiter.rlim_cur); //Print current limit
        printf("The maximum limit is: %lu \n", memlimiter.rlim_max); //print max limit 
        return 1;
    }
    else if(x==2) //if there are two tokens passed
    {
        if(atoi(tokens[1]) != 0)
        {
            int x = atoi(tokens[1]);
            memlimiter.rlim_cur = x; 
            memlimiter.rlim_max = memlimiter.rlim_cur+1; //We attempt to set the current limit to x
            if (setrlimit(RLIMIT_DATA, &memlimiter) != 0) {
                printf("ERROR: limit cannot be set to this value\n"); // If this fails, this is printed 
                return 1;
            }
            printf("Limit has been successfully set\n");
        }
        else{
            printf("ERROR: The Limit must be a positive integer\n");
        }
        return 1;
    }
    else if(x > 2)//Two many tokens to update limiter 
    {
        printf("ERROR: Invalid Number of Arguments ");
    }
    return 1;
}
int check_history(int x){ //If only one token exists, print the recorded history
    if( x ==1)
    {
        reciteHistory();
        return 1;
    }
    return 0;
}

char* get_a_line() //Reads a line 
{
	char *line = NULL; //pointer for the new line
	size_t buffer = 0; //buffer size needed for getline
	getline(&line, &buffer, stdin); 
	strtok(line, "\n"); //cuts of the line when \n is read
    //Gets the input yes to exit the program.
    check_exit(line); //check if the new input is an exit command
	return line;
}

int my_system(char *line) //Interprets the input and forks
{
    int retval =0; //value to be returned
	recordHistory(line); //record the inputted line in the system history
    int x = 0; //x records the number of tokens in the line 
	char *tail = line; //seperates the the string in pieces
    char *head;
	char *token[sizeof(line)]; 
	while((head = strtok_r(tail, " ", &tail))){ //Sepeates the line into tokens and counts how many tokens there are 
		token[x] = head;
		x++;
	}
    int is_limit =strcasecmp(token[0], "limit"); //checks if the check limit should be called
    if(is_limit==0){
        check_limit(token, x);
    }
    int is_cd =strcasecmp(token[0], "cd"); //checks if chdir should be called 
    if(is_cd ==0){
        retval = check_chdir(token,x);
        return retval;
    }
    int is_Chdir =strcasecmp(token[0], "chdir"); //This checks if chdir should be called 
    if(is_Chdir==0){
        retval =check_chdir(token,x);
        return retval;
    }
    int is_history =strcasecmp(token[0], "history"); //checks if history should be printed 
    if(is_history==0) {
        retval= check_history(x);
        return retval;
    }
	pid_t pid;
	pid = fork();
   	token[x]=NULL;
	if(pid<0){ //Fork failed
        fprintf(stderr, "ERORR: Fork was unsuccessful");
		exit(EXIT_FAILURE); //exit with a fork failure 
		return 0;
	}
	else if(pid == 0){ //Fork was successful 
		execvp(token[0], token);
    	exit(EXIT_SUCCESS);
	}
	else{ 
		wait(NULL);
	}
	return 1; //my_system concluded
}

int main(int argc, char *argv[])
{
    int infinite_loop=1;
	while(infinite_loop) //Infinite loop of reading inputs 
	{
        printf("%s", "Input: "); //Marks when new input is needed 
		signal(SIGINT, intHandler); //Signal for ^C
		signal(SIGTSTP, stpHandler); //Signal for ^Z
		char *line = get_a_line(); //get aline is called to read a line
		if(strlen(line) > 1)  //if the line is longer 
			my_system(line);
	}
	return 0;
}