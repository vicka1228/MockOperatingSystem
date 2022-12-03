#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdbool.h>
#include<sys/types.h>
#include<sys/wait.h>
#include <sys/time.h>  //this header file is here to use the gettimeofday functions in the recv_timeout user defined function
#include<sys/socket.h> 
#include<netinet/in.h>  //header for MACROS and structures related to addresses "sockaddr_in", INADDR_ANY
#include<arpa/inet.h> // header for functions related to addresses from text to binary form, inet_pton 
#include<unistd.h>  //usleep
#include<fcntl.h>   //fcntl
#include<signal.h> // header for signal related functions and macros declarations

#define PORT 5554 

#define _GNU_SOURCE
#define buffSize 2048
#define MAX_SIZE 100
#define MAXLIST 100
#define MAXCOM 100
#define CHUNK_SIZE 1024


//making sock a global variable so that we can use it in the exit handler
int sock = 0;

/*
    Receive data in multiple chunks by checking a non-blocking socket
    Timeout in seconds
*/
int recv_timeout(int s , int timeout);



// function routine of Signal Handler for SIGINT, to send connection termination message to server and terminates the client process
void clientExitHandler(int sig_num)
{
  send(sock,"exit",strlen("exit"),0); // sending exit message to server
  close(sock); // close the socket/end the conection
  printf("\n Exiting client.  \n");
  fflush(stdout);// force to flush any data in buffers to the file descriptor of standard output,, a pretty convinent function
  exit(0);
}




// Greeting shell during startup
void init_shell()
{
    
    printf("\n\n\n\n******************************************     \n");
    printf("\n\n\n**** Bhavicka and Maisha's SHELL ****\n");
    printf("\n\n\n\n******************************************  \n");
    printf("Enter \"help\" to get help and \"exit\" to exit the shell\n");
    char* username = getenv("USER");
    printf("\n\n\n USER: @  %s", username);
    printf("\n");
    sleep(1);
}

// Function to print Current Directory.
void printDir()
{
    char cwd[buffSize];
    getcwd(cwd, sizeof(cwd));
    char* username = getenv("USER");
    printf("\n   Dir : %s : @%s  ", cwd, username);
}




void commandList()
{
    printf("\n Please type any of the following commands, else you might get error \n  Command grep [keyword] does not work here \n");
    printf("\n ls ");
    printf("\n ps ");
    printf("\n pwd ");
    printf("\n rm  [from file] [to file]");
    printf("\n mkdir [newdirctory]");
    printf("\n rmdir [newdirctory]");
    printf("\n man [keyword] : for manual");
    printf("\n clear ");
    printf("\n help ");
    printf("\n exit ");
    printf("\n date ");
    printf("\n cp [from file] [to file]");
    printf("\n mv [from file] [to file]");
    printf("\n ls -lh ");
    printf("\n ls [keyword] ");
    printf("\n ls|wc");
    printf("\n pwd|wc ");
    printf("\n ls|more ");
    printf("\n ls|sort ");
    printf("\n ps|more ");
    printf("\n ps|sort ");
    printf("\n ps|sort|wc ");
    printf("\n ps|more|sort ");
    printf("\n ls|sort|wc");
    printf("\n ps|sort|wc ");
    printf("\n ps|more|wc ");
    printf("\n ls|more|wc ");
    printf("\n ls|more|sort|wc ");
    printf("\n ls|sort|more|wc ");
    printf("\n ps|sort|more|wc ");
    printf("\n ps|more|sort|wc ");
    

    
}





void help()
{
    printf("Welcome to our shell! You can enter single commands or piped commands here. A non-exhaustive list of commands includes:\nls\npwd\nman\ndate\nmkdir\nrmdir\nrm\ncp\nmv\nps\n");
    printf("You can also try out commands with single (...|...), double (...|...|...) or triple (...|...|...|...) pipes.\n");
    printf("\nPLEASE NOTE: All combinations work for single commands, but if you want to use pipes you must use single word non-combination commands. This means a|b, not a | b or a| b or a |b or a -c|b etc. Some examples you could try out are:\n");
    printf("pwd|wc\nls|wc\nls|sort\nls|more|wc\nls|more|sort|wc\n\n or any such commands in a similar vein");
}


// function for parsing command words
int parseSpace(char* inputString, char** parsedArgs )
{
    int i=0;
    
    for (i = 0; i < 1000; i++) {
        parsedArgs[i] = strsep(&inputString, " ");
  
        if (parsedArgs[i] == NULL)
                break;
        //if any of the array space is empty, store word in that array space
        if (strlen(parsedArgs[i]) == 0)
           {
               i--;
               
           }
    }
    
    return i;
    
}



int main() 
{ 

       //Set the SIGINT (Ctrl-C) signal handler to clientExitHandler 
       signal(SIGINT, clientExitHandler);
       //variables for shell
        char inputString[MAXCOM];
        char checkString[MAXCOM];
        char *parsed[MAXLIST];
        init_shell();
        commandList();
        
        //variables for client socket
        int valread; 
        struct sockaddr_in serv_addr; 
        char buffer[buffSize] = {0}; 
        
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        { 
                printf("\n Socket creation error \n"); 
                return -1; 
        } 

        serv_addr.sin_family = AF_INET; 
        serv_addr.sin_port = htons(PORT); 

     // Convert IPv4 and IPv6 addresses from text to binary form and set the ip
    //This function converts the character string 127.0.0.1 into a network
    // address structure in the af address family, then copies the
    // network address structure to serv_addr.sin_addr
       if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  // check if conversion failed
       {
         printf("\n Invalid address / Address not supported \n");
         return -1;
       }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("\n Connection Failed \n"); 
            return -1; 
        }
        int p=0, np=0;
       while(1)
       {
           int i=0;
           // printf("Enter a string:");
           //fgets(inputString, 255,stdin);
         do{
               
            printDir();
            /* code */        
            gets(inputString);
            //remove space from single line commands here as well.
            p=0; np=0;
            //exit check

            if(strcmp("exit", inputString)==0)
            {
                send(sock, inputString, strlen(inputString),0);
                p=2;
                //should we close socket here? because 
                //close(sock);
                //exit(0);
                // printf("Exiting client \n");
                // close(sock);
                // break;
            }

            //help check
            if(strcmp("help", inputString)==0)
            {
                help();
                fflush(stdout);// force to flush any data in buffers to the file descriptor of standard output,, a pretty convinent function
                continue;
            }

            //copied a new version of the input to use it for parsing to limit commands, just so inputString doesn't get changed
            for(int i=0;i<strlen(inputString);i++)
            {
                checkString[i]=inputString[i];
            }
            //1st part of the input
            int sizeParsed=parseSpace(checkString,parsed);

            //if there is no command, ask user for another input
            if (strcmp(inputString, "") == 0) 
            { //handle empty command
              printf("empty command. Input Again \n");
              p=1;

            }

            //wrong cuz input string might include cd and sth with it
            else if(strcmp(parsed[0], "cd") == 0 || strcmp(parsed[0], "ping") == 0 || strcmp(parsed[0], "man") == 0)
            {
                fprintf(stdout,"This command is not supported by our shell. Please enter some other command.\n");
                fflush(stdout);// force to flush any data in buffers to the file descriptor of standard output,, a pretty convinent function           
                p = 1;                

            }


            } while (p==1);

            if(p==2) //this means exit command has been typed
            {
                break;
                close(sock);
                exit(0);
            }
 
            //send the user inut to server
            send(sock, inputString, strlen(inputString),0);
            //receiver part
            char newstr[buffSize]={0};
            char message[buffSize]={0};
            //Now receive full data
            int total_recv = recv_timeout(sock, 4);
            //recv(sock,message,sizeof(message),0); // receive message from server
            //printf("\nClient Received message from server on socket %d, here is the message : %s\n",sock, message); // print the message received
            fflush(stdout);  
        }
	
	
	close(sock);       
	return 0; 
} 



//code inspired from https://www.binarytides.com/receive-full-data-with-recv-socket-function-in-c/
/*
    Receive data in multiple chunks by checking a non-blocking socket
    Timeout in seconds
*/
int recv_timeout(int s , int timeout)
{
    int size_recv , total_size= 0;
    struct timeval begin , now;
    char chunk[CHUNK_SIZE];
    double timediff;
    
    //make socket non blocking
    fcntl(s, F_SETFL, O_NONBLOCK);
    
    //beginning time
    gettimeofday(&begin , NULL);
    printf("\nClient Received message from server on socket %d, here is the message :\n",s);
    while(1)
    {
        gettimeofday(&now , NULL);
        
        //time elapsed in seconds
        timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
        
        //if you got some data, then break after timeout
        if( total_size > 0 && timediff > timeout )
        {
            break;
        }
        
        //if you got no data at all, wait a little longer, twice the timeout
        else if( timediff > timeout*2)
        {
            break;
        }
        
        memset(chunk ,0 , CHUNK_SIZE);  //clear the variable
        if((size_recv =  recv(s , chunk , CHUNK_SIZE , 0) ) < 0)
        {
            //if nothing was received then we want to wait a little before trying again, 0.1 seconds
            usleep(100000);
        }
        else
        {
            total_size += size_recv;
            // print the message received
            printf("%s" , chunk);
            //reset beginning time
            gettimeofday(&begin , NULL);
        }
    }
    fflush(stdout);// force to flush any data in buffers to the file descriptor of standard output,, a pretty convinent function
    
    return total_size;
}

