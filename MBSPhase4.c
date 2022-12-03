#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h> //header for MACROS and structures related to addresses "sockaddr_in", INADDR_ANY
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h> // header for signal related functions and macros declarations
#include <pthread.h>
#include <sys/mman.h> // header for shared memory specific functions declerations: mmap(), munmap()
#include <semaphore.h>
#include <limits.h> // for the value INT_MAX
#include <fcntl.h>

#define PORT 5554
#define buffSize 2048
#define _GNU_SOURCE

#define MAX_SIZE 100
#define MAXLIST 100
#define MAXCOM 100

int server_fd;
int newDummy = 0;
int quantum = 7;

// process synchronization variables
#define NUM_CLIENTS 10 // socket length of queue for pending client connections
#define SHARED 0       // 0 = thread, !0 = process-sharing

/// user defined functions
void *ThreadScheduler(void *arg);
void *HandleClient(void *new_socket);

sem_t *dummyPSemaphore; // named semaphore declared here so that we can remove it if server closes on ctrl+c signal. It opens with initial value in scheduler thread's function

// function routine of Signal Handler for SIGINT, to terminate all the threads which will all be terminated as we are calling exit of a process instead of pthread_exit
void serverExitHandler(int sig_num)
{
    printf("\n Exiting server.  \n");

    fflush(stdout); // force to flush any data in buffers to the file descriptor of standard output,, a pretty convinent function
    sem_close(dummyPSemaphore);
    sem_unlink("/dummyPSemaphore");
    close(server_fd);
    exit(0);
}

//////////////////////////////////////////
// node for the queue
//  A structure to represent a queue
//  A linked list (LL) node to store a queue entry
// reference: Data structures clss, geeks for geeks

struct QNode
{
    // int key;
    struct QNode *next;
    int threadID;
    int roundNum;
    // changing them to just semaphore, not pointer...? Will it chnage segmentation faults?
    // sem_t* semaphore;
    sem_t semaphore;
    // sem_init(&semaphore, 0, 0);
    int jobTimeRemaining; // this is to know the length of time remaining since the client went inside the
};

struct QNode *front = NULL;
struct QNode *rear = NULL;
struct QNode *current = NULL;

// add at the back, delete from the front
void enQueue(struct QNode *q)
{

    struct QNode *temp = front;
    // if list is empty, we add this node to the 1st node
    if (front == NULL)
    {
        front = q;
        printf("\n in enQueue if(front == NULL) \n");
        fflush(stdout);
    }
    else
    {
        temp = front;
        while (temp->next != NULL)
        {
            temp = temp->next;
        }
        temp->next = q;
        rear = q;

        printf("\n in enQueue else \n");
        fflush(stdout);
    }

    printf("\n End of in enQueue \n");
    fflush(stdout);
}

// code inspired from geeks for geeks Queue in C
void deleteQNode(int threadID)
{

    struct QNode *temp = front;
    struct QNode *prev = NULL; // declaring the prev cuz we need it for tracking previous thread id
    // check if node not empty, if front is the only node
    if (temp != NULL && temp->threadID == threadID)
    {
        front = temp->next;
        free(temp);
        return;
    }
    // if threadID was not present in queue
    if (temp == NULL)
    {
        return;
    }
    // as long as we don't find the desired node, keep track of previous node
    while (temp != NULL && temp->threadID != threadID)
    {
        prev = temp;
        temp = temp->next;
    }
    // we found the desired node
    // ovverwrite the node from the queue, so node removed
    prev->next = temp->next;
    free(temp);
}

// function to get the node whose threadID we know
struct QNode *getNode(int threadID)
{
    struct QNode *temp = front;

    // if my queue is empty
    if (front == NULL)
    {
        return front;
    }
    // checking if we even have anythimg in the queue
    while (temp != NULL)
    {
        if (temp->threadID == threadID)
        {
            return temp;
        }
        else
        {
            temp = temp->next; // else go to next node
        }
    }
    // if there is no ID match, we just return the front node
    return front;
}

bool isQueueEmpty()
{
    // If queue is empty, return NULL.
    if (front == NULL)
        return true;
    else
        return false;
}

// function to get SRJ with SJRF Algorithm
// reference: https: www.geeksforgeeks.org/program-for-shortest-job-first-or-sjf-cpu-scheduling-set-1-non-preemptive/
struct QNode *getSmallestJob(struct QNode *cThreadNode)
{

    struct QNode *temp = front;
    current = front;
    struct QNode *minNode = front;

    int min = INT_MAX;
    int minID;

    printf(" \nInside getSmallestJob \n");
    for(current = front; current != NULL; current = current->next)
  {
    if (current->jobTimeRemaining < minNode->jobTimeRemaining)
    {
      temp = current;
    }
  }
  return temp;

}

typedef struct pthread_arg_t
{
    int new_socket_fd;
    struct sockaddr_in client_address;
    /* TODO: Put arguments passed to threads here. See lines 116 and 139. */
} pthread_arg_t;

// code inspired from https://www.faceprep.in/c/program-to-remove-spaces-from-a-string/
void remove_white_spaces(char *str)
{
    int i = 0, j = 0;
    for (i = 0; i < strlen(str); i++)
    {

        if (str[i] != ' ')
        {
            str[j++] = str[i];
        }
    }
    if (j > 0)
    {
        str[j] = 0;
    }
}

// to remove leading and trailing white spaces, code taken from codeforwin.org
void trim(char *str)
{
    int index, i;

    /*
     * Trim leading white spaces
     */
    index = 0;
    while (str[index] == ' ' || str[index] == '\t' || str[index] == '\n')
    {
        index++;
    }

    /* Shift all trailing characters to its left */
    i = 0;
    while (str[i + index] != '\0')
    {
        str[i] = str[i + index];
        i++;
    }
    str[i] = '\0'; // Terminate string with NULL

    /*
     * Trim trailing white spaces
     */
    i = 0;
    index = -1;
    while (str[i] != '\0')
    {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
        {
            index = i;
        }

        i++;
    }

    /* Mark the next character to last non white space character as NULL */
    str[index + 1] = '\0';
}

// code ispired from  https://stackoverflow.com/questions/7143878/how-to-remove-quotes-from-a-string-in-c
void remove_special_chars(char *token)
{
    int j = 0;
    for (int i = 0; i < strlen(token); i++)
    {
        if (token[i] != '\'' && token[i] != '\"' && token[i] != '\\')
        {
            token[j++] = token[i];
        }
        else if (token[i + 1] == '\'' && token[i + i] != '\"' && token[i] == '\\')
        {
            token[j++] = '\'';
        }
        else if (token[i + 1] != '\'' && token[i + i] != '\"' && token[i] == '\\')
        {
            token[j++] = '\\';
        }
    }
    if (j > 0)
    {
        token[j] = 0;
    }
}

// function for parsing command words
void parseSpace(char *inputString, char **parsedArgs)
{
    int i = 0;
    // removes special characters from input
    remove_special_chars(inputString);

    for (i = 0; i < MAXLIST; i++)
    {
        parsedArgs[i] = strsep(&inputString, " ");

        if (parsedArgs[i] == NULL)
            break;
        // if any of the array space is empty, store word in that array space
        if (strlen(parsedArgs[i]) == 0)
            i--;
    }
}

// function for finding pipe
int parsePipe(char *inputString, char **parsedArgsPiped)
{

    int i;
    for (i = 0; i < MAXLIST; i++)
    {

        parsedArgsPiped[i] = strsep(&inputString, "|");
        if (parsedArgsPiped[i] == NULL)
        {
            break;
        }
    }
    // there is no second argument means no pipe found, so returns zero
    if (parsedArgsPiped[1] == NULL)
    {
        return 0;
    }
    else
    {
        // we return i-1 cuz due to looping, i increases by 1
        return i - 1;
    }
}

// Function where the system command is executed
void execArgs(char **parsedArgs, int newsocket)
{
    // Forking a child
    pid_t pid = fork();

    if (pid < 0)
    {
        printf("\n  Failed forking child in execArgs");
        return;
    }
    else if (pid == 0)
    {
        dup2(newsocket, 1);
        char execmsg[buffSize] = {0};
        if (strcmp(parsedArgs[0], "mkdir") == 0 || strcmp(parsedArgs[0], "rm") == 0 || strcmp(parsedArgs[0], "rmdir") == 0 || strcmp(parsedArgs[0], "cp") == 0 || strcmp(parsedArgs[0], "mv") == 0 || strcmp(parsedArgs[0], "cat") == 0)
        {
            int j = snprintf(execmsg, buffSize, "Something has been created or removed using server using socket : %d !", newsocket); // puts string into buffer
            send(newsocket, execmsg, strlen(execmsg), 0);                                                                             // send message to client
            // This function is used to set all the socket structures with null values
            bzero(execmsg, buffSize);
        }
        close(newsocket);
        if (execvp(parsedArgs[0], parsedArgs) < 0)
        {
            printf("\n Could not execute command [in function execArgs] ");
            return;
        }
    }
    else
    {

        // waiting for child to terminate
        wait(NULL);

        return;
    }
}

// Function where the piped system commands is executed
void execArgsPiped1(char **parsedArgsPiped, int newsocket)
{
    // 0 is read end, 1 is write end
    int fd[2];
    pid_t p1, p2;
    if (pipe(fd) < 0)
    {
        printf("\n  Piping Failed ");
        return;
    }

    p1 = fork();
    if (p1 < 0)
    {
        printf("\n Could not fork");
        return;
    }
    else if (p1 == 0)
    {
        // Child 1 executing
        // redirecting the standard output
        dup2(fd[1], 1);
        // writing done, so closing end
        close(fd[1]);
        // It only needs to write at the write end, so we close the read end 0
        close(fd[0]);

        // using a seperate array of strings so that I can add NULL at the end of the vector sent to execvp

        char *piped1_0[20] = {0};                 // we set all the elems to 0 so we can check for how many string elems we have in the array
        parseSpace(parsedArgsPiped[0], piped1_0); // parse the first element of the parsed pipe array, to support compound commands
        // //printf("Reaching here\n");
        // //printf("%s, %s\n", piped1_0[0], piped1_0[1]);
        int y = 0;
        int length1 = 0;
        while (piped1_0[length1] != 0)
        {
            length1++;
        }

        // setting the first non-character empty space to NULL
        for (int y = length1; y < 20; y++)
        {
            piped1_0[y] = NULL;
        }

        if (execvp(piped1_0[0], piped1_0) < 0)
        {
            printf("\n Compound Command Not Allowed For Pipes ");
            printf("\n Execvp failed child 1");
            return;
        }
    }
    else
    {
        // Parent executing
        p2 = fork();
        if (p2 < 0)
        {
            printf("\n Could not fork ");
            return;
        }
        // Child 2 executing

        else if (p2 == 0)

        {
            dup2(fd[0], 0);
            close(fd[0]);
            // It only needs to read at the read end, so closing the write end 1
            close(fd[1]);
            char *piped1_1[20] = {0};                 // we set all the elems to 0 so we can check for how many string elems we have in the array
            parseSpace(parsedArgsPiped[1], piped1_1); // parse the first element of the parsed pipe array, to support compound commands; similar process for other piped commands too
            int length2 = 0;
            while (piped1_1[length2] != 0)
            {
                length2++;
            }
            // piped1_1[2]=NULL;
            for (int y = length2; y < 20; y++)
            {
                piped1_1[y] = NULL;
            }
            // redirecting standard output of the last end to client
            dup2(newsocket, 1);
            close(newsocket);
            if (execvp(piped1_1[0], piped1_1) < 0)
            {
                printf("\n Compound Command Not Allowed For Pipes ");
                printf("\n   Execvp failed  child 2 ");
                return;
            }
        }
        else
        {
            // parent executing, waiting for two children
            close(fd[0]);
            close(fd[1]);
            // wait(NULL);
        }
        // wait(NULL);
    }
}

void execArgsPiped2(char **parsedpipe, int newsocket)
{

    int fd1[2]; // pipe 1 for getting output from child 1 and giving it to child 2
    int fd2[2]; // pipe 2 for getting output from child 2 and giving it to parent
    if (pipe(fd1) < 0)
    {
        printf("\n  Piping 1 Failed ");
        return;
    }
    if (pipe(fd2) < 0)
    {
        printf("\n  Piping 2 Failed ");
        return;
    }

    int pid;
    pid = fork();
    if (pid < 0)
    {
        return;
    }

    else if (pid == 0)
    {

        dup2(fd1[1], 1); // write by redirecting standard output to pipe 1
        close(fd1[1]);
        close(fd1[0]);
        close(fd2[0]);
        close(fd2[1]);

        char *piped2_0[20] = {0}; // we set all the elems to 0 so we can check for how many string elems we have in the array
        parseSpace(parsedpipe[0], piped2_0);
        // printf("Reaching here\n");
        // printf("%s, %s\n", piped1_0[0], piped1_0[1]);
        int y = 0;
        int length1 = 0;
        while (piped2_0[length1] != 0)
        {
            length1++;
        }

        for (int y = length1; y < 20; y++)
        {
            piped2_0[y] = NULL;
        }

        if (execvp(piped2_0[0], piped2_0) < 0)
        {

            printf("\n Compound Command Not Allowed For Pipes ");
            perror("Execvp failed in command 1 ");
            return;
        }
    }
    else
    {
        pid = fork();

        if (pid < 0)
        {
            printf("\n Could not fork ");
            return;
        }

        else if (pid == 0)
        {
            dup2(fd1[0], 0); // reading redirected ouput of ls through pipe 1
            dup2(fd2[1], 1); // write by redirecting standard output to pipe 2
            close(fd1[1]);
            close(fd1[0]);
            close(fd2[1]);
            close(fd2[0]);
            // char* piped2_1[3];   //using a seperate array of strings so that I can add NULL at the end of the vector sent to execvp
            // piped2_1[0]=parsedpipe[1];
            // piped2_1[1]=NULL;

            char *piped2_1[20] = {0}; // we set all the elems to 0 so we can check for how many string elems we have in the array
            parseSpace(parsedpipe[1], piped2_1);
            int length2 = 0;
            while (piped2_1[length2] != 0)
            {
                length2++;
            }
            // piped1_1[2]=NULL;
            for (int y = length2; y < 20; y++)
            {
                piped2_1[y] = NULL;
            }

            if (execvp(piped2_1[0], piped2_1) < 0)
            {
                printf("\n Compound Command Not Allowed For Pipes ");
                perror("Execvp failed while command 2");
                return;
            }
        }
        else
        {
            pid = fork();
            if (pid == 0)
            {
                dup2(fd2[0], 0); // reading redirected ouput of child 2 through pipe 2
                close(fd1[1]);
                close(fd1[0]);
                close(fd2[1]);
                close(fd2[0]);
                // char* piped2_2[3]; //using a seperate array of strings so that I can add NULL at the end of the vector sent to execvp
                // piped2_2[0]=parsedpipe[2];
                // piped2_2[1]=NULL;

                char *piped2_2[20] = {0}; // we set all the elems to 0 so we can check for how many string elems we have in the array
                parseSpace(parsedpipe[2], piped2_2);
                int length3 = 0;
                while (piped2_2[length3] != 0)
                {
                    length3++;
                }
                // piped1_1[2]=NULL;
                for (int y = length3; y < 20; y++)
                {
                    piped2_2[y] = NULL;
                }

                // redirecting standard output of the last end to client
                dup2(newsocket, 1);
                close(newsocket);

                if (execvp(piped2_2[0], piped2_2) < 0)
                {
                    printf("\n Compound Command Not Allowed For Pipes ");
                    perror("Execvp failed while executing command 3");
                    return;
                }
            }
            else
            {
                close(fd1[1]);
                close(fd1[0]);
                close(fd2[1]);
                close(fd2[0]);

                // wait(NULL);
            }

            // wait(NULL);
        }

        // wait(NULL);
    }
    return;
}

void execArgsPiped3(char **parsedpipe, int newsocket)
{

    int fd1[2]; // pipe 1 for getting output from child 1 and giving it to child 2
    int fd2[2]; // pipe 2 for getting output from child 2 and giving it to child 3
    int fd3[2]; // pipe 3 for getting output from child 3 and giving it to parent
    if (pipe(fd1) < 0)
    {
        printf("\n  Piping 1 Failed ");
        return;
    }
    if (pipe(fd2) < 0)
    {
        printf("\n  Piping 2 Failed ");
        return;
    }
    if (pipe(fd3) < 0)
    {
        printf("\n  Piping 3 Failed ");
        return;
    }

    int pid1 = fork();
    if (pid1 < 0)
    {
        printf("\n Could not fork");
        return;
    }
    else if (pid1 == 0)
    {

        dup2(fd1[1], 1); // write by redirecting standard output to pipe 1
        close(fd1[1]);
        close(fd1[0]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd3[0]);
        close(fd3[1]);
        // char* piped3_0[3]; //using a seperate array of strings so that I can add NULL at the end of the vector sent to execvp
        // piped3_0[0]=parsedpipe[0];
        // piped3_0[1]=NULL;

        char *piped3_0[20] = {0}; // we set all the elems to 0 so we can check for how many string elems we have in the array
        parseSpace(parsedpipe[0], piped3_0);
        // printf("Reaching here\n");
        // printf("%s, %s\n", piped1_0[0], piped1_0[1]);
        int y = 0;
        int length1 = 0;
        while (piped3_0[length1] != 0)
        {
            length1++;
        }

        for (int y = length1; y < 20; y++)
        {
            piped3_0[y] = NULL;
        }

        if (execvp(parsedpipe[0], piped3_0) < 0)
        {
            printf("\n Compound Command Not Allowed For Pipes ");
            perror("Execvp failed in command 1 ");
            return;
        }
    }
    else
    {
        int pid2 = fork();

        if (pid2 < 0)
        {
            printf("\n Could not fork ");
            return;
        }

        else if (pid2 == 0)
        {

            dup2(fd1[0], 0); // reading redirected ouput of ls through pipe 1
            dup2(fd2[1], 1); // write by redirecting standard output to pipe 2
            close(fd1[1]);
            close(fd1[0]);
            close(fd2[1]);
            close(fd2[0]);
            close(fd3[0]);
            close(fd3[1]);

            char *piped3_1[20] = {0}; // we set all the elems to 0 so we can check for how many string elems we have in the array
            parseSpace(parsedpipe[1], piped3_1);
            int length2 = 0;
            while (piped3_1[length2] != 0)
            {
                length2++;
            }
            // piped1_1[2]=NULL;
            for (int y = length2; y < 20; y++)
            {
                piped3_1[y] = NULL;
            }

            if (execvp(piped3_1[0], piped3_1) < 0)
            {
                printf("\n Compound Command Not Allowed For Pipes ");
                perror("Execvp failed while command 2");
                return;
            }
        }
        else
        {
            int pid3 = fork();

            if (pid3 < 0)
            {
                printf("\n Could not fork ");
                return;
            }

            if (pid3 == 0)
            {
                dup2(fd2[0], 0); // reading redirected ouput of ls through pipe 2
                dup2(fd3[1], 1); // write by redirecting standard output to pipe 3
                close(fd1[1]);
                close(fd1[0]);
                close(fd2[1]);
                close(fd2[0]);
                close(fd3[0]);
                close(fd3[1]);

                char *piped3_2[20] = {0}; // we set all the elems to 0 so we can check for how many string elems we have in the array
                parseSpace(parsedpipe[2], piped3_2);
                int length2 = 0;
                while (piped3_2[length2] != 0)
                {
                    length2++;
                }
                // piped1_1[2]=NULL;
                for (int y = length2; y < 20; y++)
                {
                    piped3_2[y] = NULL;
                }
                if (execvp(piped3_2[0], piped3_2) < 0)
                {
                    printf("\n Compound Command Not Allowed For Pipes ");
                    perror("Execvp failed while command 3");
                    return;
                }
            }

            else
            {
                int pid4 = fork();

                if (pid4 < 0)
                {
                    printf("\n Could not fork ");
                    return;
                }

                if (pid4 == 0)
                {
                    dup2(fd3[0], 0); // reading redirected ouput of child 3 through pipe 3
                    close(fd1[1]);
                    close(fd1[0]);
                    close(fd2[1]);
                    close(fd2[0]);
                    close(fd3[0]);
                    close(fd3[1]);

                    char *piped3_3[20] = {0}; // we set all the elems to 0 so we can check for how many string elems we have in the array
                    parseSpace(parsedpipe[3], piped3_3);
                    int length2 = 0;
                    while (piped3_3[length2] != 0)
                    {
                        length2++;
                    }

                    for (int y = length2; y < 20; y++)
                    {
                        piped3_3[y] = NULL;
                    }
                    // redirecting standard output of the last end to client
                    dup2(newsocket, 1);
                    close(newsocket);
                    execvp(piped3_3[0], piped3_3);
                    printf("\n Compound Command Not Allowed For Pipes ");
                    perror("Execvp failed while executing command 4");
                    return;
                }
                else
                {
                    close(fd1[1]);
                    close(fd1[0]);
                    close(fd2[1]);
                    close(fd2[0]);
                    close(fd3[0]);
                    close(fd3[1]);
                }
            }
        }
    }

    return;
}

// int main(int argc, char const *argv[])
int main()
{

    // Set the SIGINT (Ctrl-C) signal handler to serverExitHandler
    signal(SIGINT, serverExitHandler);

    // variables
    int new_socket;
    struct sockaddr_in address;
    // int valread=0;
    int addrlen = sizeof(address);
    pthread_attr_t pthread_attr;
    pthread_arg_t *pthread_arg;
    // pthread_t pthread;
    socklen_t client_address_len;

    // //semaphores
    // sem_init(&empty, SHARED, 1);
    // sem_init(&full, SHARED, 0);
    // sem_init(&sm,SHARED,1);  //this is my semaphore

    /* Initialise IPv4 address. */

    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Creating socket file descriptor with communication: domain of internet protocol version 4, type of SOCK_STREAM for TCP socket, protocol of the internet
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    /* Bind address to socket. */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    /* Assign signal handlers to signals. */
    if (signal(SIGINT, serverExitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }

    /* Listen on socket. */
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    /* Initialise pthread attribute to create detached threads. */
    if (pthread_attr_init(&pthread_attr) != 0)
    {
        perror("pthread_attr_init");
        exit(1);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0)
    {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }
    //   char message[buffSize]={0};
    // thread to scheduler fun ction
    pthread_t scheduler_id;
    int rc_scheduler;
    rc_scheduler = pthread_create(&scheduler_id, NULL, ThreadScheduler, NULL);
    if (rc_scheduler) // if rc is > 0  or <0 imply could not create new thread
    {
        printf("\n ERROR: return code from pthread_create is %d \n", rc_scheduler);
        exit(EXIT_FAILURE);
    }

    while (1) // to keep server alive for infintiy
    {

        pthread_arg = (pthread_arg_t *)malloc(sizeof *pthread_arg);
        if (!pthread_arg)
        {
            perror("malloc");
            continue;
        }
        /* Accept connection to client. */
        client_address_len = sizeof pthread_arg->client_address;

        // checkExit=0;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&pthread_arg->client_address, &client_address_len)) < 0)
        {
            perror("accept failed");
            free(pthread_arg);
            continue;
            exit(EXIT_FAILURE);
        }

        /* Initialise pthread argument. */
        pthread_arg->new_socket_fd = new_socket;

        int rc; // return value from pthread_create to check if new thread is created successfukky

        // we are not using thread_socket anymore  , instead using the struc  pthread_arg
        int *thread_socket = (int *)malloc(sizeof(int)); // for passing safely the integer socket to the thread
        if (thread_socket == NULL)
        {
            fprintf(stderr, "Couldn't allocate memory for thread new socket argument.\n");
            exit(EXIT_FAILURE);
        }
        *thread_socket = new_socket;
        //////////////////////////////////////////

        // if previous thread exited, i.e flag=1, we enter to create a new thread to execute a new command
        pthread_t client_id; // client's ID (just an integer, typedef unsigned long int) to indetify new thread
        // create a new thread that will handle the communication with the newly accepted client
        rc = pthread_create(&client_id, &pthread_attr, HandleClient, (void *)pthread_arg);
        if (rc) // if rc is > 0  or <0 imply could not create new thread
        {
            printf("\n ERROR: return code from pthread_create is %d \n", rc);
            exit(EXIT_FAILURE);
        }
    }
    // we didn't close the client socket new_socket cuz every new client will be handled by a new thread

    // closing the sockets
    /* close(socket_fd);
     * TODO: If you really want to close the socket, you would do it in
     * signal_handler(), meaning socket_fd would need to be a global variable.
     */
    close(server_fd);
    // pthread_exit(NULL);       // terminate the main thread
    free(pthread_arg);
    return 0;
}

// Function that handles the client thread
void *HandleClient(void *arg)
{

    // using a struct for the thread arguments
    pthread_arg_t *pthread_arg = (pthread_arg_t *)arg;
    int socket = pthread_arg->new_socket_fd;
    struct sockaddr_in client_address = pthread_arg->client_address;

    free(arg);
    // pthread_detach(pthread_self()); // detach the thread as we don't need to synchronize/join with the other client threads, their execution/code flow does not depend on our termination/completion
    // int socket = *(int*)new_socket;
    // if(new_socket!=NULL){free(new_socket);}
    printf("handling new client in a thread using socket: %d\n", socket);
    printf("Listening to client....\n"); // while printing make sure to end your strings with \n or \0 to flush the stream, other wise if in anyother concurent process is reading from socket/pipe-end with standard input/output redirection, it will keep on waiting for stream to end.

    // semaphore for threads, now every thread will have a semaphore
    // sem_t* thread_semaphore=malloc(sizeof(sem_t));

    sem_t thread_semaphore;
    printf("\n after sem_t thread_semaphore \n"); // print the message received
    fflush(stdout);
    sem_init(&thread_semaphore, 0, 0); // this is my semaphore initialized to 0, meaning every client thread can access it., the value is 1, so we can wait first on thread_semaphore
    // to check if this thread is a dummy and then running/has run before
    printf("\n after thread_semaphore \n"); // print the message received
    fflush(stdout);

    int isMyDummyRunning = 0;
    char message_cpy[buffSize];
    char *dummyCommands[MAXLIST];

    char buffer[buffSize] = {0};
    char inputString[255];

    printf("\n between \n"); // print the message received
    fflush(stdout);

    // variables for shell
    char checkString[MAXCOM], *parsedArgs[MAXLIST], *parsedArgs2[MAXLIST], *parsedArgs3[MAXLIST], *parsedArgs4[MAXLIST];
    char *parsedArgsPiped[MAXLIST];
    int execFlag = 0;
    int pipe_number = -1;
    int valread = 0;

    // allocating the memory dynamically, this heap memory is share between parent and child
    int *checkExit; // since it has been declared int he parent, the child wil have a copy of it which should get updated when the value in parent gets updated
    checkExit = (int *)malloc(sizeof(int));
    *checkExit = 0;
    printf("\n Accepted New Socket, Waiting for New Client \n"); // print the message received

    // if dummy program is not running, then it could be the start of a dummy program or shell command
    // if dummy running, that means we will need scheduer, so only then store the details of the thread inside the queue, else no need cuz shell command will not be taken to queue for scheduling

    while (1)
    {

        int waitStatus;
        char *message = (char *)malloc(buffSize * sizeof(char));
        bzero(message, buffSize);
        // clearing the message arrays properly
        memset(message, 0, buffSize * (sizeof(char)));
        valread = read(socket, message, buffSize);

        if (valread < 0)
        {
            printf("failure in receiving data in server in main \n");
            fflush(stdout);
            exit(EXIT_FAILURE);
            // printf("HI   \n");//this line never prints cuz it exits
        }
        fflush(stdout); // force to flush any data in buffers to the file descriptor of standard output

        // code modified from codegrepper.com
        if (strcmp(message, "exit") == 0)
        {
            // handle exit command
            printf("Server terminating in child\n");
            fflush(stdout);
            *checkExit = 1;
            // send the user inut to server
            int p = send(socket, message, strlen(message), 0);
            printf("p:  %d", p);
            if (p < 0)
            {
                perror("exit msg sending failed");
                exit(EXIT_FAILURE);
            }
            break;
            // pthread_exit(NULL);
            // reference: https://stackoverflow.com/questions/27798419/closing-client-socket-and-keeping-server-socket-active
            // shutdown(socket, SHUT_RDWR);
            // close(socket);
            // close(pthread_arg->new_socket_fd);

            // pthread_exit(NULL);// terminate the thread //it is not terminating the thread
            // break; //if I use break, the child process runs but they don't enter this exit comparison anymore
        }

        // isMyDummyRunning is a local variable, you will find it way up at the beginning of this function
        if (isMyDummyRunning == 0)
        {
            // if dummy program is not running, then it could be the start of a dummy program or shell command

            strcpy(message_cpy, message);
            parseSpace(message_cpy, dummyCommands); // divide the commands into 3 parts
            // we ceate a copy and work n te copy cuz else the main variable will be edited
            // char *checkdummy = strtok(message_cpy, " ");
            // checks whether dummy program
            if (strcmp(dummyCommands[0], "./dummyProgram.o") == 0)
            {

                char errMsg[buffSize] = "Thread ID and Job time parameter required to run ./dummyProgram.o. Please enter ./dummyProgram.o with Thread ID and job time parameter \n";
                char *thread_id = dummyCommands[1];
                char *jobTimeRemainingStr = dummyCommands[2];

                if (jobTimeRemainingStr == NULL)
                {
                    fflush(stdout);
                    send(socket, errMsg, strlen(errMsg), 0);
                    continue;
                }

                printf("before jobTimeRemaining = atoi(jobTimeRemainingStr)");
                fflush(stdout);
                // send(socket, errMsg, strlen(errMsg), 0);
                int jobTimeRemaining = atoi(jobTimeRemainingStr);

                printf("before struct QNode* program = NULL;");
                fflush(stdout);
                // send(socket, errMsg, strlen(errMsg), 0);
                //  send(socket, errMsg, strlen(errMsg), 0);
                // dynamically allocating so it gets shared over the threads
                struct QNode *program;
                program = (struct QNode *)malloc(sizeof(struct QNode *));
                program->threadID = pthread_self();
                program->jobTimeRemaining = jobTimeRemaining; // this initial value is the burst time for RR
                program->roundNum = 1;
                program->semaphore = thread_semaphore;
                program->next = NULL;
                printf("before enqueue");
                // add to queue
                enQueue(program);
                printf("after enqueue");
                // set global flag
                newDummy = 1;
                // set bool value
                isMyDummyRunning = 1;

                sem_wait(&thread_semaphore); // making this semaphore wait until it's unlocked by thread scheduler
                printf("after -  sem_wait(thread_semaphore); ");
            }
            // this bracket is for the "if (strcmp(checkdummy, "./dummyProgram.o") == 0)""

            // else // if the input string is not dummy program, go over to the shell command
            // {

            //     // forking the child
            // }
        }
        else if (isMyDummyRunning == 1) // if this current thread/ program has run before, we get it from the queue to update it's remaining job time
        {
            // get thread idof our program ,
            int threadID = pthread_self();
            // this variable gets the remaining job time for running dummy
            struct QNode *dummyNodeRunningJobRemaining = getNode(threadID);
            int jobTimeRemainingofMyDummyRunning = dummyNodeRunningJobRemaining->jobTimeRemaining;
            char howmuchleft[buffSize];
            sprintf(howmuchleft, "Thread ID: %d, running for an iteration. Remaining time: %d \n", threadID, jobTimeRemainingofMyDummyRunning);
            send(socket, howmuchleft, strlen(howmuchleft), 0);
            sleep(1);
            sem_wait(&dummyNodeRunningJobRemaining->semaphore);
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            printf("failure in forking inside main \n");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // child process, perform reading from socket here
            printf("\n Server Received message from client on socket %d, here is the message : %s\n", socket, message); // print the message received
            fflush(stdout); // force to flush any data in buffers to the file descriptor of standard output
            //if the user did not run a dummy command                                                                                           
            if (isMyDummyRunning == 0 && strcmp(dummyCommands[0], "./dummyProgram.o") != 0)
            {
                // redircting the standard output in the child
                dup2(socket, STDOUT_FILENO);
                dup2(socket, STDERR_FILENO); // close(new_socket);
            }

            // if there is pipe, no special characters or spaces allowed
            for (int i = 0; i < strlen(message); i++)
            {
                if (message[i] == '|')
                {
                    // removes spaces from input
                    // remove_white_spaces(message);
                    // removes leading and trailing white spaces from input
                    trim(message);
                    // removes special characters from input
                    remove_special_chars(message);
                }
            }

            // if my current client is not  running a dummy currently, only then go to execute shell command
            if (isMyDummyRunning == 0)
            {
                pipe_number = parsePipe(message, parsedArgsPiped);
                if (pipe_number == 0)
                {
                    parseSpace(message, parsedArgs);
                    execArgs(parsedArgs, socket);
                }
                else if (pipe_number == 1)
                {
                    execArgsPiped1(parsedArgsPiped, socket);
                }
                else if (pipe_number == 2)
                {
                    execArgsPiped2(parsedArgsPiped, socket);
                }
                else if (pipe_number == 3)
                {
                    execArgsPiped3(parsedArgsPiped, socket);
                }
                else
                {
                    printf(" \n Please try again: No compound commands please, also no space before and after the commands are typed with pipes for example: a|b, not a | b or a| b or a |b etc. or try the \"help\" command for a detailed list of commands ");
                }
            }
        }
        // in parent
        else
        {
            wait(&waitStatus);
            // wait(NULL);
            printf("\n *checkExit in parent : %d", *checkExit);
            fflush(stdout);
            // clientQueue->front->semaphore=1;
            if (*checkExit == 1)
            {
                printf("Server terminating in parent\n");
                break;
                // close(socket);
            }

            // this part takes back jobTimeRemaining detail from the dummy execution which decrements jobTimeRemaining
            if (isMyDummyRunning == 1)
            {
                int dummyRemainingTime = WEXITSTATUS(waitStatus);
                // here, get the job in the queue and update the remaining time/ round robin
                int threadID = pthread_self();
                struct QNode *job = getNode(threadID);
                job->jobTimeRemaining = dummyRemainingTime;
                job->roundNum = job->roundNum + 1;

                if (dummyRemainingTime == 0)
                {
                 // program done executing!
                    char dummyFinishMessage[buffSize] = "./dummyProgram.o execution completed \n";
                    send(socket, dummyFinishMessage, strlen(dummyFinishMessage), 0);
                    isMyDummyRunning = 0;
                    // deletes the completed node from the process queue
                    deleteQNode(threadID);
                }
                // release the semaphore so that scheduler can take over control
                sem_post(&thread_semaphore);
                
            }

            // this part if for the time when a shell command is getting executed and dummy is still running in the background
        }

        free(message);
        fflush(stdout);

        // munmap(checkExit, sizeof(int)); // in common area, call munmap from child and parent both and unmap the mapping (shared memory object) from their respective address spaces

    } // bracket for the while loop

    sem_destroy(&thread_semaphore);
    free(checkExit);
    pthread_exit(NULL); // terminate the thread
}

// refence for scheduling

// referenece: http://cs.franklin.edu/~shaffstj/cs345/week4.htm

// refenece: https://www.stechies.com/round-robin-scheduling/
// this is only to schedule the dummy programs
void *ThreadScheduler(void *arg)
{

    pthread_detach(pthread_self());

    // printf("Inside ThreadScheduler"); //gets printed
    // fflush(stdout);

    // refenece: https://unix.stackexchange.com/questions/651965/how-to-open-an-existing-named-semaphore
    // we ned this when executing the dummy program

    dummyPSemaphore = sem_open("/dummyPSemaphore", O_CREAT, 0644, 0); // starts with value 0, so we can post on it first
    int checkIfOldProgramInQueue = 0;
    struct QNode *cThreadNode = NULL; // currently running thread/client node which is not in the scheduler

    while (1)
    {

        sleep(1);

        // printf("Inside While of ThreadScheduler"); //gets printed
        // fflush(stdout);

        // If queue empty, no scheduling required, start over for scheduling
        if (isQueueEmpty() == true)
        {
            printf("\n Queue Empty \n");
            fflush(stdout);
            // continue;
        }
        else
        {
            printf("\n Quantum: %d \n", quantum);
            printf("\n newDummy: %d \n", newDummy);
            fflush(stdout);

            // if a new dummy program was created
            if (quantum <= 0 || newDummy == 1)
            {

                printf("Inside quantum <= 0 || newDummy == 1 of ThreadScheduler");
                fflush(stdout);

                if (quantum <= 0)
                {
                    printf("\n Quantum ended. \n");
                    fflush(stdout);
                }
                if (newDummy == 1)
                {
                    printf("\n New dummy program entered the queue \n");
                    fflush(stdout);
                }

                newDummy = 0;

                // resetting quantum
                quantum = 7;
                printf("\n Scheduling new task......... \n");
                fflush(stdout);
                // it should hav ebeen null f it was a new one, this means it's not a new one
                if (cThreadNode != NULL)
                {
                    printf("\n inside if (cThreadNode != NULL)   1   \n");
                    // sem_t currentSemaphore = cThreadNode->semaphore;
                    //  sem_init(&currentSemaphore, 0, 1); // starts with value 1, so we can post on it first
                    sem_wait(dummyPSemaphore);         // in wait, value decrements to -1, therefore, the dummy program waits in the infinite loop of wait
                    sem_wait(&cThreadNode->semaphore); // in wait, value decrements to -1, therefore, the creently running semaphore stops
                    printf("\n inside if (cThreadNode != NULL)   2   \n");
                    fflush(stdout);
                }

                printf("\n after if (cThreadNode != NULL)  \n");
                // we select the next node that will run run after execution of the SJRF algorithm, except for the current node
                // here the function looks for the  node with the smallest remaining job time
                struct QNode *nextSmallestThreadNode = getSmallestJob(cThreadNode);
                // sem_t threadSemaphore = nextSmallestThreadNode->semaphore;
                //  sem_init(&threadSemaphore, 0, 0);
                printf("\n nextSmallestThreadNode->jobTimeRemaining : %d", nextSmallestThreadNode->jobTimeRemaining);
                printf("\n before posting sem_post(&threadSemaphore); and  sem_post(dummyPSemaphore);  \n");
                fflush(stdout);
                // releasing semaphore locks
                sem_post(&nextSmallestThreadNode->semaphore); // releasing lock of the thread with smallest job
                sem_post(dummyPSemaphore);                    // releasing loack of the dummpy program, now the decrementing job time remainining can work from the dummy program
                // the moment I opened the lock, now dummy program is decrementing the job time
                printf("\n after posting sem_post(&threadSemaphore); and  sem_post(dummyPSemaphore);  \n");
                fflush(stdout);
                cThreadNode = nextSmallestThreadNode;
                // this variable saves the initial job time of the program before job time gets decreemented

                // while(1)
                // {
                //       if((nextSmallestThreadNode->jobTimeRemaining)==timeLeft-quantum)
                //       {

                //         break;

                //       }
                // }
            }
            else
            {
                quantum--;
                printf("\n inside quantum decrementing  \n");
                fflush(stdout);
            }

            //  // Store previous front and move front one node ahead
            //  struct QNode* temp = clientQueue->front;
            //  //semaphore sent in waiting.
            //  clientQueue->front->semaphore=0;
            //  q->front = q->front->next;

            //  // If front becomes NULL, then change rear also as NULL
            //  if (q->front == NULL)
            //     { q->rear = NULL;}
            //  // free(temp);
            // return temp;
            //     struct QNode* firstNode;
            //     firstNode=deQueue(clientQueue);
            //     firstNode->semaphore=0;
        }
    }
}

// Not used really
// Queue
//  The queue, front stores the front node of LL and rear stores the
//  last node of LL
//  struct Queue {
//      struct QNode *front, *rear;
//  };

// // A utility function to create a new linked list node.
// struct QNode* newNode(int threadID, int roundNum, sem_t semaphore, int jobTimeRemaining)
// {
//     struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode));
//     temp->threadID = threadID;
//     temp->roundNum = roundNum;
//     temp->semaphore = semaphore;
//     temp->jobTimeRemaining = jobTimeRemaining;
//     temp->next = NULL;
//     return temp;
// }

// // A utility function to create an empty queue
// struct Queue* createQueue()
// {
//     struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
//     q->front = q->rear = NULL;
//     return q;
// }

// The function to add a key k to q
// void enQueue(struct Queue* q, int threadID, int roundNum, sem_t semaphore, int jobTimeRemaining)
// {
//     // Create a new LL node
//     struct QNode* temp = newNode(threadID, roundNum, semaphore, jobTimeRemaining);

//     // If queue is empty, then new node is front and rear both
//     if (q->rear == NULL) {
//         q->front = q->rear = temp;
//         return;
//     }
//     // Add the new node at the end of queue and change rear
//     q->rear->next = temp;
//     q->rear = temp;
// }

// // Function to remove a key from given queue q
// struct QNode* deQueue(struct Queue* q)
// {
//     // If queue is empty, return NULL.
//     if (q->front == NULL)
//         return;

//     // Store previous front and move front one node ahead
//     struct QNode* temp = q->front;

//     q->front = q->front->next;

//     // If front becomes NULL, then change rear also as NULL
//     if (q->front == NULL)
//        { q->rear = NULL;}
//     // free(temp);
//    return temp;
// }

// struct Queue* clientQueue=createQueue();
// this is where all the incoming clients are gonna come and be stored

// int getNode(int threadID)
// {

// }

////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// round robin
// reference:https://www.stechies.com/round-robin-scheduling/

// void getSystem()
// {
//     int i;

//     printf("\nThe Quantum: ");
//     scanf("%d", &quantum);

//     for(i=0; i<NP; i++ )
//     {
//         printf("\n Arrival Time of process%d: ", i);
//         scanf("%d", &processes[i][0]);
//         printf("\n Burst time for process%d: ", i);
//         scanf("%d", &processes[i][1]);
//         processes[i][2] = processes[i][1];
//         printf("\n-----------");
//     }
// }

// //Printing the output for which process is running and how much remaining for the process
// void printSystem()
// {
//     int i;
//     printf("\n\n******************************************************************");
//     printf("\nQuantum Time: %d",quantum);
//     printf("\nProcess:  Arrival-Time  Burst-Time Remaining-Time");
//     for(i=0; i<NP; i++)
//     {
//         printf("\nProcess%d:  \t%d  \t\t%d  \t\t%d", i, processes[i][0], processes[i][1], processes[i][2]);
//         printf("\n______________________________________________________");
//     }
//     printf("\nProcess in the Queue: ");
//     Q *n;
//     for(n=queue; n!=NULL; n=n->next)
//     {
//         printf("Process%d ",n->p);
//     }
// }

// //Function to get remaining time for the process

// unsigned int executionRemained()
// {
//     int i;
//     unsigned int x = 0;
//     for(i=0; i<NP; i++)
//     {
//         if(processes[i][2] > 0)
//         {
//             x = 1;
//         }
//     }
//     return x;
// }
