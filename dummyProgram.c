#include <unistd.h> 
#include <stdio.h>  
#include <stdlib.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <stdbool.h> 
#include <pthread.h> 
#include <signal.h> 
#include <fcntl.h> 
#include <semaphore.h> 





int main(int argc, char *argv[])
{


  if (argc != 3) 
  {
    printf("2 parameters: threadID and job time parameter required\n");
    return -1;
  }

  int threadID= atoi(argv[1]);
  int jobTimeRemaining = atoi(argv[2]);

  // refenece: https://unix.stackexchange.com/questions/651965/how-to-open-an-existing-named-semaphore
  // we ned this when executing the dummy program
  sem_t *dummyPSemaphore;
  dummyPSemaphore = sem_open("/dummyPSemaphore", O_CREAT, 0644, 1);


  while(1) 
  {
    int sem_val = 1;
    int returnValue = sem_getvalue(dummyPSemaphore, &sem_val);
    
    if (sem_val == 1) //which mens the lock is open, so critical section accessible
    {

        jobTimeRemaining=jobTimeRemaining-1;  //why is code not coming here?
        printf("Thread ID %d running, time remaining %d \n", threadID, jobTimeRemaining);
        sleep(1);
    } 

    else 
    {
      if(jobTimeRemaining<=0)
      { 
          return 0; //we do not want a negative job time value 
      }
      else
      {   return jobTimeRemaining;

      }

    }
    
  }
  
  sem_close(dummyPSemaphore);
  return 0;
}