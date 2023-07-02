//This program serves as a shell interface that accepts user commands
//and then executes each command in a separate process.
//This program supports input and output redirection and pipes 
//as a form of IPC between a pair of commands.

//Assumptions and Implementation
//"&" at end of command allows child process to run in background
//"!!" executes the most recent command
//Command will contain only one pipe character and will not be
//		combined with any redirection operators

//-------------------UNIX Shell Interface-----------------------

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>

#define MAX_LINE 80 //max length command
char *args[MAX_LINE/2 + 1]; //command/arguments
int should_run = 1; //tracks if user enter "exit"
int ELEMENT = 0; //tracks number of elements in args
char history[30]; //copy of userInput for "!!" situations
char userInput[30];
int repeat = 0; //helper variable for multiple calls of "!!"

//---------------------------inputProcessing---------------------------------------------------------
//Processes the input to be properly formatted for execution
//Returns a bool value of whether the command should be processed and executed
bool inputProcessing() {
	ELEMENT = 0; //starts over with each new command
	for(int i = 0; i < MAX_LINE/2+1; i++) { 
		args[i] = 0;  //making args values null
 	}

	printf("osh>");
	fflush(stdout);

	fgets(userInput, sizeof(userInput), stdin); //get user input

	char *tok;
	if(userInput[0] == '!' && userInput[1] == '!') {
		if(history[0] == 0 && history[1] == 0) { //first command is "!!"
			printf("No commands in history.\n");
			return false;
		}
		tok = strtok(history, " "); 
		repeat++;
	} else {
		strcpy(history, userInput); //copy userInput to history
		tok = strtok(userInput, " ");
		repeat = 0;
	}
	
	while(tok!= NULL) {
		args[ELEMENT] = tok; //entering separated cmd/arg into args
		ELEMENT++;
		tok = strtok(NULL, " ");
	}
	if(repeat <= 1) { //if there is a repeat of "!!" don't remove last char
		args[ELEMENT-1][strlen(args[ELEMENT-1])-1] = '\0';
	}
	return true;
}

//------------------------charTest--------------------------------------------------
//Helper method that tests of a char(in this case: <, >, |)
//Takes parameter char[]
//If found, returns index value of the char in args
//If not found, returns -1
int charTest(char c[]) {
	int test;
	for(int i = 0; i <ELEMENT; i++) {
		test = strcmp(args[i], c);
		if(test == 0) {
			return i;
		}
	}
	return -1;
}

//---------------------processIO------------------------------------------------------
//Processes the necessary steps before executing redirection commmands
//Takes two parameters:
//	loc = int index value of location of the < or > in args
//  in = bool value of whether the redirection is for input or output
//Returns goAhead of whether to continue execution of command
bool processIO(int loc, bool in) {
	int fd;
	int elementRange = ELEMENT;
	char *fName = args[loc+1];
	if(in == true) { //input redirection 
		mode_t mode1 = S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR;
		fd = open(fName, O_RDONLY, mode1);
		if(fd != -1) {
			dup2(fd, STDIN_FILENO); //file will serve as input to command
		} else {
			printf("Error:File Not Found.\n");
			return false;
		}
		
	} else { //output redirection
		mode_t mode = S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR;
		fd = open(fName, O_WRONLY|O_CREAT|O_TRUNC, mode);
		dup2(fd, STDOUT_FILENO); //output of command will go into file
	}
	for(int i = loc; i < elementRange; i++) {
		args[i] = 0; //gets rid of filename from args so correct command will execute
		ELEMENT--;
	}
	return true;
}

//---------------------------------processPipe----------------------------------------------
// Processes and executes pipe command
// Takes parameter loc, which is an int index value of | in args
void processPipe(int loc) {
	char* secArgs[MAX_LINE/2 + 1];
	int elementRangeTwo = ELEMENT;
	int j = 0;
	for(int i = loc + 1; i < elementRangeTwo; i++) {
		secArgs[j] = args[i];
		args[i] = 0; //deleting 2nd command from args
		j++;
	}
	args[loc] = 0; //gets rid of pipe in args
	for(int i = j; i < MAX_LINE/2 + 1; i++) {
		secArgs[i] = 0; //fill rest of secArgs with null values
	}

	int fd[2];
	pipe(fd); 
	pid_t pid = fork(); 
	if(pid == 0) { //child
		pid_t pid2 = fork();
		if(pid2 == 0) {  //child's child, grandchild
			close(fd[0]);  //close read end
			dup2(fd[1], STDOUT_FILENO); //1st command is outputting into the write end of pipe
			int status2 = execvp(args[0], args); //left/1st command
			if(status2 == -1) {
				printf("Execution error.");
			}
		} else { 
			wait(NULL); //wait until child/grandchild done 
			close(fd[1]); //close write end
			dup2(fd[0], STDIN_FILENO); //2nd commandis taking the input in from the read end of pipe
			int status = execvp(secArgs[0], secArgs); //right/2nd half of pipe
			if(status == -1) {
				printf("Exectuion error.");
			}
		}
	} else { //parent
		 close(fd[0]); //close read end
		 close(fd[1]); //close write end
		wait(NULL);
	}
}
 
//--------------------------------processCommand---------------------------------------------------------
//Processes input from inputProcess
//Executes all commmands except for pipe which executes from its own method above
void processCommand() {
	int saved_out = dup(1); //saving stdin and stidout for reset
	int saved_in = dup(0);
	bool goAhead = true;

	//charTests for redirection and pipes
	char inp[] = "<";
	int inputLoc = charTest(inp);
	char out[] = ">";
	int outputLoc = charTest(out);
	char p[] = "|";
	int pLoc = charTest(p);
	
	char s1[] = "exit";
	int c = strcmp(args[0], s1);
	if(c == 0) { //user entered "exit"
		should_run = 0;
	} 
	else {
		if(inputLoc != -1) { //input char detected
	 		goAhead = processIO(inputLoc, true);
	 	} 
		if(outputLoc != -1) { //output char detected
	 		processIO(outputLoc, false);
	 	} 

		bool waitForChild = true;
		if(ELEMENT > 0) { //only check for & if there args values
			char s2[] = "&";
			int d = strcmp(args[ELEMENT-1], s2);
			if (d == 0) { //user entered & 
				waitForChild = false; //flag that parent will not wait
				args[ELEMENT-1] = 0;  //remove & from args before execution
			}  
		}

		if(pLoc != -1) { //detected pipe
			processPipe(pLoc); //process and execute pipe commands
		} else if (goAhead == false) {
		}
		else { //execute all other commands
			pid_t pid = fork();
			if(pid == 0) {  //child
				execvp(args[0], args);
			}
			else { //parent
				if(waitForChild == true) { 
					int status;
					pid_t pidW = wait(&status); 
					while(pidW != -1) { //wait for all children
						pidW = wait(&status);
					}
				}
			}
		}
	}
	dup2(saved_out, 1); //resetting stdin and stdout
	close(saved_out);
	dup2(saved_in, 0);
	close(saved_in);
}

//-------------------------------main--------------------------------------------
int main(void) {
	while(should_run) {	 //while user doesn't enter "exit"
		bool goProcess = false;
		goProcess = inputProcessing();
		if(goProcess == true) { //as long as goProcess gives go ahead
			processCommand();
		}
	}
		return 0;
}

