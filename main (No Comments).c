#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <wait.h>

#define readEnd 0
#define writeEnd 1


struct termios OT;

void restoreTerminal();
char** initHistoryArray(char* s);
char** stringToArray(char* s); 
char** stringToArray2(char* s);
size_t getLengthOf2D(char** array); 
void printArray();  
char* handleInput(char* userInput, char** arr);
bool searchArg(char* s, char** arg);
int posOfString(char* s, char** arg); 
char* nullifyNewLine(char* s); 
void splitCommand(char** arr, char*** firstCmd, char*** secondCmd); 

int main(){   
    char initialDirectory[500] =""; 
    char currentDirectory[500] = ""; 
    getcwd(initialDirectory, sizeof(initialDirectory)); 
    char historyString[1000000] = ""; 
    char* args = ""; 
    char** historyArray = NULL;
    char** cmdArray = NULL;
    bool cdTriggered = false;
    bool pipeAttempted = false;
    bool inputRedirectionAttempted = false; 
    bool outputRedirectionAttempted = false; 

    while(1){
        if(strcmp(historyString, "") != 0){ 
            historyArray = initHistoryArray(historyString); 
        };
        char UI[500] = ""; 
        if(cdTriggered) {
            char WD[500] = ""; 
            getcwd(WD, sizeof(WD)); 
            if(strcmp(initialDirectory, WD) != 0){
                char** filepath = stringToArray2(WD); 
                printf("blamOSH:%s>", filepath[getLengthOf2D(filepath)-1]); 
                fflush(stdout); 
            } else {
                cdTriggered = false; 
                printf("blamOSH>"); 
                fflush(stdout); 
            }
        } else {
            printf("blamOSH>"); 
            fflush(stdout);
        }
        args = handleInput(UI, historyArray);
        printf("\n");
        if(strcmp(args, "exit") == 0){
            break;
        }
        cmdArray = stringToArray(args); 
        if(cmdArray){
            if(strcmp(cmdArray[0], "cd") == 0){
                if(getLengthOf2D(cmdArray) > 2){
                    printf("cd: too many arguments\n");
                } else {
                    chdir(cmdArray[1]);
                    cdTriggered = true;
                }
            }
        }
        pid_t pid = fork();
        if(pid == 0){
            if(strcmp(args, "!!") == 0){
                if(historyArray){
                    args = historyArray[getLengthOf2D(historyArray) - 1];
                } 
                else {
                    printf("No commands in history\n");
                    exit(0);
                }
            }
            if(strcmp(args, "history") == 0){
                if(historyArray){
                    printArray(historyArray);
                } else {
                    printf("No commands in history\n");
                }
                exit(0);
            }
            cmdArray = stringToArray(args); 
            pipeAttempted = searchArg("|", cmdArray);
            if(pipeAttempted){
                int fd0[2];
                char** cmd1 = NULL;
                char** cmd2 = NULL;
                splitCommand(cmdArray, &cmd1, &cmd2);
                pipe(fd0);
                int input_fd;
                int output_fd; 
                pid_t pid1 = fork(); 
                if(pid1 == 0){
                    inputRedirectionAttempted = searchArg("<", cmd1); 
                    outputRedirectionAttempted = searchArg(">", cmd1);
                    if(inputRedirectionAttempted && outputRedirectionAttempted){
                        printf("Too many arguments\n");
                        exit(0);
                    }
                    if(inputRedirectionAttempted) {
                        int pos = posOfString("<", cmd1);
                        if(pos + 1 < getLengthOf2D(cmd1)) {
                            char* inputFile = cmd1[pos + 1]; 
                            input_fd = open(inputFile, O_RDONLY); 
                            dup2(input_fd, STDIN_FILENO);
                            close(input_fd);
                            close(output_fd); 
                            cmd1[pos] = NULL;
                        }
                    }
                    if(outputRedirectionAttempted) {
                        int pos = posOfString(">", cmd1); 
                        if(pos + 1 < getLengthOf2D(cmd1)) {
                            char* outputFile = cmd1[pos + 1];
                            fd0[writeEnd] = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2(output_fd, STDOUT_FILENO); 
                            close(output_fd);
                            close(input_fd); 
                            cmd1[pos] = NULL; 
                        }
                    }
                    close(fd0[readEnd]);
                    dup2(fd0[writeEnd], STDOUT_FILENO);
                    close(fd0[writeEnd]);
                    if(strcmp(cmd1[0], "cd") == 0){
                        exit(0);
                    }
                    if(execvp(cmd1[0], cmd1) == -1){
                        printf("%s: command not found\n", cmd1[0]);
                        exit(0);
                    }
                } else {
                    close(fd0[writeEnd]); 
                    dup2(fd0[readEnd], STDIN_FILENO); 
                    close(fd0[readEnd]); 
                    inputRedirectionAttempted = searchArg("<", cmd2); 
                    outputRedirectionAttempted = searchArg(">", cmd2);
                    if(inputRedirectionAttempted && outputRedirectionAttempted){
                        printf("Too many arguments\n");
                        exit(0);
                    }
                    if(inputRedirectionAttempted) {
                        int pos = posOfString("<", cmd2); 
                        if(pos + 1 < getLengthOf2D(cmd2)) {
                            char* inputFile = cmd2[pos + 1]; 
                            input_fd = open(inputFile, O_RDONLY); 
                            dup2(input_fd, STDIN_FILENO); 
                            close(input_fd); 
                            cmd2[pos] = NULL; 
                        }
                    }
                    if(outputRedirectionAttempted) {
                        int pos = posOfString(">", cmd2); 
                        if(pos + 1 < getLengthOf2D(cmd2)) {
                            char* outputFile = cmd2[pos + 1];  
                            output_fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2(output_fd, STDOUT_FILENO); 
                            close(output_fd); 
                            cmd2[pos] = NULL; 
                        }
                    }
                    if(strcmp(cmd2[0], "cd") == 0){
                        exit(0);
                    }
                    if(execvp(cmd2[0], cmd2) == -1){
                        printf("%s: command not found\n", cmd2[0]);
                        exit(0);
                    }
                }
            } else {
                int input_fd; 
                int output_fd; 
                inputRedirectionAttempted = searchArg("<", cmdArray); 
                outputRedirectionAttempted = searchArg(">", cmdArray); 
                if(inputRedirectionAttempted && outputRedirectionAttempted){
                        printf("Too many arguments");
                        exit(0);
                }

                if(inputRedirectionAttempted) {
                    int pos = posOfString("<", cmdArray); 
                    if(pos + 1 < getLengthOf2D(cmdArray)) {
                        char* inputFile = cmdArray[pos + 1]; 
                        input_fd = open(inputFile, O_RDONLY); 
                        dup2(input_fd, STDIN_FILENO); 
                        close(input_fd); 
                        cmdArray[pos] = NULL; 
                    }
                }
                if(outputRedirectionAttempted) {
                    int pos = posOfString(">", cmdArray); 
                    if(pos + 1 < getLengthOf2D(cmdArray)) {
                        char* outputFile = cmdArray[pos + 1]; 
                        output_fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666); 
                        dup2(output_fd, STDOUT_FILENO); 
                        close(output_fd); 
                        cmdArray[pos] = NULL;
                    }
                }
                if(strcmp(cmdArray[0], "cd") == 0){
                    exit(0);
                }
                if(execvp(cmdArray[0], cmdArray) == -1){
                    printf("%s: command not found\n", cmdArray[0]);
                    exit(0);
                }
            }
        } else if (pid > 0){
            wait(NULL); 
            if(strcmp(args, "") != 0 && strcmp(args, "!!") != 0){
                strcat(historyString, args); 
                strcat(historyString, "*"); 
            }
        }
    }
    chdir(initialDirectory); 
}


void restoreTerminal(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &OT); 
}


char** initHistoryArray(char* s){
    char** theArray = NULL; 
    char* separator;
    char* strCopy = strdup(s); 
    int size = 0; 
    separator = strtok(strCopy, "*"); 
    while (separator != NULL) {
        size++; 
        theArray = (char**) realloc(theArray, sizeof(char*) * (size + 1)); 
        theArray[size - 1] = strdup(separator); 
        theArray[size] = NULL; 
        separator = strtok(NULL, "*"); 
    }

    free(strCopy); 
    return theArray; 
}

char** stringToArray(char* s){
    char** theArray = NULL;
    char* separator;
    char* strCopy = strdup(s);
    int size = 0;

    separator = strtok(strCopy, " ");
    while (separator != NULL) {
        size++;
        theArray = (char**) realloc(theArray, sizeof(char*) * (size + 1));
        theArray[size - 1] = strdup(separator);
        theArray[size] = NULL;
        separator = strtok(NULL, " ");
    }

    free(strCopy);
    return theArray;
}

char** stringToArray2(char* s){
    char** theArray = NULL;
    char* separator;
    char* strCopy = strdup(s);
    int size = 0;

    separator = strtok(strCopy, "/");
    while (separator != NULL) {
        size++;
        theArray = (char**) realloc(theArray, sizeof(char*) * (size + 1));
        theArray[size - 1] = strdup(separator);
        theArray[size] = NULL;
        separator = strtok(NULL, "/");
    }

    free(strCopy);
    return theArray;
}

size_t getLengthOf2D(char** array){
    size_t length = 0; 
    if(array){
        while(array[length] != NULL){
            length++;
        }
    }
    return length;
}

void printArray(char** array){
    size_t length = getLengthOf2D(array);
    for(int i = 0; i < length; i++){
        printf("%d\t\t%s\n", i, array[i]);
    }
}

char* handleInput(char *userInput, char** arr){
    char* returnString = ""; 
    struct termios CT; 
    char up_arrow[] = "\033[A";
    char down_arrow[] = "\033[B";

    
    tcgetattr(STDIN_FILENO, &OT); 
    CT = OT; 
    CT.c_lflag &= ~(ICANON | ECHO);
    CT.c_cc[VMIN] = 1; 
    CT.c_cc[VTIME] = 0; 

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &CT); 

    char* temp = NULL;
    char* temp2 = NULL;

    int length = 0;
    int numOfKeyPresses = 0;
    int historyLength = getLengthOf2D(arr);

    
    while(1){
        char c; 
        read(STDIN_FILENO, &c, 1); 
        if (c == '\033') {
            char seq[3];
            read(STDIN_FILENO, &seq[0], 1); 
            read(STDIN_FILENO, &seq[1], 1); 
            seq[2] = '\0'; 
            if(arr != NULL) {
                if (strcmp(seq, "[A") == 0 && ((historyLength - 1) - numOfKeyPresses) > -1) {
                    if(numOfKeyPresses == 0){
                        temp2 = strdup(userInput);
                    }
                    for(int i = 0; i < length; i++){
                        printf("\b \b");
                    }
                    temp = arr[(historyLength - 1) - numOfKeyPresses];
                    temp = nullifyNewLine(temp); 
                    strcpy(userInput, temp); 
                    length = strlen(userInput); 
                    printf("%s", userInput);
                    fflush(stdout); 
                    numOfKeyPresses++; 
                } else if (strcmp(seq, "[B") == 0) { 
                    numOfKeyPresses--; 
                    for(int i = 0; i < length; i++){
                        printf("\b \b");
                    }
                    if(numOfKeyPresses < 1){
                        if (temp2 != NULL) {
                            strcpy(userInput, temp2); 
                            length = strlen(userInput); 
                            printf("%s", userInput); 
                            fflush(stdout); 
                        }
                        numOfKeyPresses = 0;
                    } else if(numOfKeyPresses > 0){
                        temp = arr[historyLength - numOfKeyPresses]; 
                        temp = nullifyNewLine(temp); 
                        strcpy(userInput, temp);
                        length = strlen(userInput); 
                        printf("%s", userInput); 
                        fflush(stdout); 
                    }
                }
            }
            continue; 
        } else if (c == 127) { 
            if (length > 0) {
                userInput[--length] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
        }
        else if (c == '\n') {
            userInput[length] = '\0';
            returnString = userInput;
            break;
        } else {
            if (length < 500 - 1) {
                userInput[length++] = c; 
                putchar(c);
                fflush(stdout); 
            }
        }
    }
    restoreTerminal(); 
    free(temp); 
    free(temp2); 
    return returnString; 
}


bool searchArg(char* s, char** arg){
    bool wasFound = false;
    int len = getLengthOf2D(arg); 
    for(int i = 0; i < len; i++){
        if(strcmp(s, arg[i]) == 0) {
            wasFound = true;
        }
    }
    return wasFound; 
}

int posOfString(char* s, char** arg){
    int pos = 0; 
    int len = getLengthOf2D(arg); 
    for(int i = 0; i < len; i++){
        if(strcmp(s, arg[i]) == 0) {
            pos = i;
            break;
        }
    }
    return pos;
}


char* nullifyNewLine(char* s){
    int len = strlen(s); 
    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
    return s;
}


void splitCommand(char** arr, char*** firstCmd, char*** secondCmd){
    int pipeIndex = posOfString("|", arr); 
    int len = getLengthOf2D(arr); 
    *firstCmd = (char**)malloc((pipeIndex+1) * sizeof(char*)); 
    *secondCmd = (char**)malloc((len - pipeIndex) * sizeof(char*)); 
    memcpy(*firstCmd, arr, pipeIndex * sizeof(char*));
    memcpy(*secondCmd, &arr[pipeIndex + 1], (len - pipeIndex-1) * sizeof(char*)); 
    (*firstCmd)[pipeIndex] = NULL;
    (*secondCmd)[len - pipeIndex - 1] = NULL; 
}