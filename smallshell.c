#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>


#define DELIMS " \n"
#define TRUE 1
#define FALSE 0
#define MAXARGS 512
#define MAXLINE 2048

char* inputFile;
char* outputFile;
int bgProcess = FALSE;
int fgLock = FALSE;

//reads and stores raw user input in a string
char* getEntry(){
	char *userEntry = NULL;
	ssize_t buffer = 0;
	getline(&userEntry, &buffer, stdin);		

	//printf("recorded string from getEntry(): %s\n", userEntry);fflush(stdout);
	return userEntry;
}

//cuts entry string into an array of strings to pass to execvp
char** parseEntry(char* entry){
	int pos = 0;
	int buffer = MAXARGS;
	char* token;
	char** args = malloc(buffer * sizeof(char*));			//allocate space for 512 possible arguments
	
	token = strtok(entry, DELIMS);					//cut off the new line
	//printf("delimiters cut off; processing %s\n", token);fflush(stdout);

	while(token){
	//printf("processing string token: %s\n", token);fflush(stdout);


	if(strcmp(token, "<") == 0){					//redirect stdin
		token = strtok(NULL, DELIMS);				//ignore < charater 
		inputFile = strdup(token);				//save file name for later
		//printf("saved input file: %s\n", inputFile);fflush(stdout);
		token = strtok(NULL, DELIMS);				//advance token
	}

	else if(strcmp(token, ">") == 0){				//redirect stdout - same process as above
		token = strtok(NULL, DELIMS);
		outputFile = strdup(token);
		//printf("saved output file: %s\n", outputFile);fflush(stdout);
		token = strtok(NULL, DELIMS);
	}

	else if(strcmp(token, "&") == 0){				//process will be run in the background

		if((token = strtok(NULL, DELIMS)) == NULL){		//check that & is the last character of the entry
			if(!fgLock){					//make sure that we aren't in foreground only mode
			bgProcess = TRUE;				//mark bg flag as true
			//printf("& at end of line");fflush(stdout);
			break;						//end line parse, since & is the last character
			}						
		}

	}

	else{
		args[pos] = token;					//push argument to array and increment position
		pos++;
	
		token = strtok(NULL, DELIMS);				//get next piece of the line
		}	
	}
	
	args[pos] = NULL;						//tack on a null terminator so we know the line has been processed

	//printf("line processed now printing\n"); fflush(stdout);
	//debug print array------------------------------------------------------------
	//int j;
	//for(j = 0; j < pos; j++){
	//	printf("array pos %d: %s\n", j, args[j]);fflush(stdout);
	//}

	return args;
}

//reads the exit status of the last process and prints its value
//adapted from lecture 3.1
void checkStatus(int status){

	if(WIFEXITED(status)){						//if process terminated normally, then evaluates to non 0
	int exitStatus = WEXITSTATUS(status);				//convert to actual status 		
	printf("exit value %d\n", exitStatus);fflush(stdout);		//display to user
	}
	else{								//else process was terminated by a signal
	int termSignal = WTERMSIG(status);
	printf("terminated by signal %d\n", termSignal);fflush(stdout);
	}
}


//responds to ctrl Z to toggle foreground only mode
void toggleFG(int signo){

	if(!fgLock){							//activate foreground only mode
	fgLock = TRUE;
	char* fgEnter = "Entering fg only mode\n";			//notify user
	write(1, fgEnter, 22);fflush(stdout);				//non-reentrant print
	}

	else if(fgLock){						//deactivate forground only mode
	fgLock = FALSE;
	char* fgExit = "Exiting fg only mode\n";			//notify user
	write(1, fgExit, 21);fflush(stdout);				//non-reentrant print
	}
}


int main(){

int lock = TRUE;							//loop user entry until exit
int infd, outfd;							//variables for checking successful redirections
int processStatus = 0;							//set initial status as 0

int shellPID = getpid();						//get pid for parent shell
//printf("shell PID: %d\n", shellPID);fflush(stdout);

char* pid;
sprintf(pid, "%d", shellPID);						//convert pid int to string
//printf("string pid: %s\n", pid);fflush(stdout);

char* pidstr; 								//holds $$ string for checking later


struct sigaction csignal = {0}, zsignal = {0};				//initialize signal structs to empty

//sig handler ^C
csignal.sa_handler = SIG_IGN;						//ignore ctrl c
sigfillset(&csignal.sa_mask);						//block/delay other signals
csignal.sa_flags = 0;

//sig handler ^Z
zsignal.sa_handler = toggleFG;						//route to forground function
sigfillset(&zsignal.sa_mask);						//block/delay other signals
zsignal.sa_flags = 0;

sigaction(SIGINT, &csignal, NULL);					//register the signals
sigaction(SIGTSTP, &zsignal, NULL);

while(lock){								//begin shell loop

	pid_t cpid = waitpid(-1, &processStatus, WNOHANG);		//check for completed processes
	//printf("cpid from top of loop: %d\n", cpid); fflush(stdout);
	if(cpid > 0){
	printf("background pid %d is done\n", cpid);
	checkStatus(processStatus); fflush(stdout);
	}
	
	printf(": "); fflush(stdout);					//colon for prompt

	char *entry = getEntry();					//read user command
	//printf("main: %s\n", entry);fflush(stdout);

	pidstr = strstr(entry, "$$");					//find any instand of $$

	if(pidstr) {							//if found, convert to parent shell pid
		strcpy(pidstr, pid);			
		//printf("converted entry: %s", entry);fflush(stdout);
	}

	char **arguments = parseEntry(entry);				//convert to array of arguments

	if(arguments[0] == NULL){					//if input is empty, restart loop
		continue;						
	}

	else if(strncmp(arguments[0], "#", 1) == 0){			//check first byte for comment indicator
		continue;						//restart loop
	}

	else if(strcmp(arguments[0], "exit") == 0){			//exit the shell and indicate successful termination
		//printf("no newline exit\n");fflush(stdout);
		exit(0);
	}

	else if (strcmp(arguments[0], "cd") == 0){			//change directories
		if(arguments[1]){					//move to argument, if provided
			chdir(arguments[1]);		
		}
		else{
			chdir(getenv("HOME"));				//otherwise just go to home directory
		}
		//printf("change dirs called\n");fflush(stdout);
	}

	else if(strcmp(arguments[0], "status") == 0){			//prints the exit status of the last process
		checkStatus(processStatus);	
	}

	else{
		pid_t cpid = -2;					//start with junk value for error checking
		cpid = fork();						//initiate fork and test results

		switch(cpid) {

			case -1: {					//error: no child process created
				perror("fork error");
				exit(1);
		 	break;
			}

			case 0: {					//during the child process
				csignal.sa_handler = SIG_DFL;		//ignore ctrl C
				sigaction(SIGINT, &csignal, NULL);

				if(inputFile){								//check if input file was specified
					infd =  open(inputFile, O_RDONLY);

					if(infd == -1){							//check for error opening
					printf("error: can't open %s for input\n", inputFile);fflush(stdout);
					exit(1);
					} 

					if(dup2(infd, 0) == -1){					//duplicate file descriptor and check for error
					printf("error: can't redirect %s for input\n", inputFile);fflush(stdout);
					exit(1);
					}

					//printf("file passed for reading %s\n", inputFile);fflush(stdout);
					close(infd);
				}


				if(outputFile){									//check for output file
					outfd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 744);		//make/open file for writing

					if(outfd == -1){							//check for error opening
					printf("error: can't open %s for output\n", outputFile);fflush(stdout);
					exit(1);
					} 

					if(dup2(outfd, 1) == -1){						//duplicate file descriptor and check for error
					printf("error: can't redirect %s for input\n", outputFile);fflush(stdout);
					exit(1);
					}

					//printf("file passed for writing %s\n", outputFile);fflush(stdout);
					close(outfd);
				}

				//printf("executing execvp\n");fflush(stdout);
				if(execvp(arguments[0], arguments)){					//if execvp returns something, then there was an error	
					printf("%s: no such file or directory\n", arguments[0]);fflush(stdout);
					exit(1);
				}
			break;
			}

			default:{									
				if(bgProcess) {
					pid_t ppid = waitpid(cpid, &processStatus, WNOHANG);		//check if child process has completed
					printf("background pid is %d\n", cpid);fflush(stdout); 

				}
				else {
					pid_t checkpid = waitpid(cpid, &processStatus, 0);		//block the parent until the child terminates
					//printf("executing normal default\n");fflush(stdout);
					//printf("finished executing pid: %d\n", checkpid);fflush(stdout);
				}
			break;
			}
		}//end of switch
	}//end of else	

	free(entry);				//deallocate memory and reset flags for next loop iteration
	free(arguments);
	inputFile = NULL;
	outputFile = NULL;
	bgProcess = FALSE;
	//printf("end of loop hit\n");fflush(stdout);
	
}//end while

return 0;
}
