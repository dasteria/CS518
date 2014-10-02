/*
   
  Please add -pthread when you compile with gcc. 

 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>



int procCount;                /* Counter of forks */
int ticks,warmup;             /* Variable for measuring CPU time */
int _vertice;	              /* Number of vertices for the graph we are calculating */
int _thread;                  /* Number of threads to compute in parallel */
int jWar,kWar;                /* Loop variables for Warshall's algorithm */

FILE *my_file;                /* Pointer to input file */
char buf[] = "graph.in";
int outX,outY;
key_t shmkey;                 /* key to be passed to shmget() */
int shmflg;                   /* shmflg to be passed to shmget() */
int shmid;                    /* return value from shmget() */
sem_t *sem;                   /*      synch semaphore         */
int matrixSize;               /* size to be passed to shmget() */
int** matrix;                 /* pointer to the matrix representation of graph */
int warmup;


key_t shmkey_i;
int shmflg_i;
int shmid_i;
int *iWar;                    /* Parallel computed loop variable in shared memory */

/*
* List of methods needed
*/

void Create_shm(void);	// Create shared memory to store matrix representation of the graph
void Ini_graph(int** mat); // Read input file, number of vertices and 2D transition matrix
void Comp_TranCls(int numVer, int** mat); // Take the 2D matrix and change it to the transitive closure matrix result
void Print_graph(void);	// Output the graph to verify result


// Start of rdstc clock reading
static __inline__ unsigned long long rdtsc_begin(void)
{
  unsigned cycles_high,cycles_low;

 __asm__ __volatile__
(
"CPUID\n\t"/*serialize*/
"RDTSC\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t": "=r" (cycles_high), "=r"
(cycles_low):: "%rax", "%rbx", "%rcx", "%rdx"); 
  return ( (unsigned long long)cycles_low)|( ((unsigned long long)cycles_high)<<32);
}

// End of rdstc clock reading
static __inline__ unsigned long long rdtsc_end(void)
{

  unsigned cycles_high1,cycles_low1;
 __asm__ __volatile__
(

"RDTSCP\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t"
"CPUID\n\t": "=r" (cycles_high1), "=r"
(cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx");
  return ( (unsigned long long)cycles_low1)|( ((unsigned long long)cycles_high1)<<32 );
}

void Create_shm(void)
{
  int numThread,numVertex;
  my_file = fopen (buf, "r");
  fscanf (my_file, "%i", &numThread);
  fscanf (my_file, "%i", &numVertex);
  printf("\nNumber of threads: %i\n",numThread);
  printf("Number of vertice: %i\n",numVertex);
  _thread = numThread;
  _vertice = numVertex;
  shmkey = ftok ("/dev", 1);
  shmkey_i= ftok("/dev/disk",1);
  printf ("Matrix shmkey = %d\n\n", shmkey);
  printf ("i shmkey = %d\n\n",shmkey_i);
  int size = numVertex * sizeof(int*) + numVertex*numVertex * sizeof(int*) + sizeof(int*);
  shmid = shmget (shmkey, size, 0666 | IPC_CREAT);
  shmid_i = shmget(shmkey_i,sizeof(int*),0666 | IPC_CREAT);
  // printf ("The matrix(%p) is allocated in shared memory.\n", matrix);
  if (shmid < 0 || shmid_i<0)
    {
      perror ("shmget\n");
      exit (1);
    }

  matrix = (int **) shmat (shmid, NULL, 0); /* attach matrix to shared memory */
  iWar = (int*) shmat(shmid_i,NULL,0);
  printf ("The matrix(%p) is allocated in shared memory.\n\n", matrix);
  printf ("The i loop variable(%p) is allocated in shared memory.\n\n",iWar);
}


void Ini_graph(int** matrix)
{
  int x,y,start,end;
  int *head = (int*)(matrix+_vertice);
  int i;
  for(i=0;i<_vertice;i++)
    {
      matrix[i] = head + i*_vertice;
      printf("The pointer for Matraix[%d] is %p \n",i,matrix[i]);
    }

  *iWar = 0;

  for(x=0;x<_vertice;x++)
    {
      for(y=0;y<_vertice;y++)
	{	  
	  if(x==y)
	    matrix[x][y]=1;
	  else
	    matrix[x][y] = 0;
	}
    }

// Iterate through inputs and save 1 for each direct path
 while(fgets(buf,10,my_file)!=NULL)
   {
     fscanf(my_file,"%i %i",&start,&end);
     //printf("This pair of input is %i %i \n",start,end);
     matrix[start-1][end-1] = 1;
   }
     fclose (my_file);

   printf("\nMatrix initialization complete.\n");
}



int main()
{
  pid_t pid;
  unsigned long long tickStart,tickEnd;
  Create_shm();
  Ini_graph(matrix);
  // Print out input graph matrix for checking purpose
  printf("\nInitial graph matrix: \n");
  for(outX=0;outX<_vertice;outX++)
    {
      for(outY=0;outY<_vertice;outY++)
	{
	  printf("%d ",matrix[outX][outY]);
	}
      printf("\n");
    }

    /* initialize semaphores for shared processes */
    // Semaphore type : unnamed
    // pshared = 10, shared amoung processes
    // initial semaphore value: 1
    //if(sem_init(&sem,0,1) == 0 )
    // printf("\n\nFailed to initialize unnamed semaphore.\n\n");
    // else
    // printf ("\n\nInitialized unnamed semaphore, share between processes. \n\n");


// the absolute path to the semaphore
//const char sem_name[] = "/tmp/sem";

// 0644 is the permission of the semaphore
//sem = sem_open(sem_name, O_CREAT, 0644, 1);
//printf("\n\n Semaphore opend.\n\n");

    /* initialize semaphores for shared processes */
    sem = sem_open ("pSem", O_CREAT | O_EXCL, 0644, 1); 
    /* name of semaphore is "pSem", semaphore is reached using this name */
    sem_unlink ("pSem");      
    /* unlink prevents the semaphore existing forever */
    /* if a crash occurs during the execution         */
    printf ("semaphores initialized.  \n\n");

// Warmup for RDSTC
rdtsc_begin();
rdtsc_end();
rdtsc_begin();
rdtsc_end();
rdtsc_begin();
rdtsc_end();
rdtsc_begin();
rdtsc_end();
rdtsc_begin();
rdtsc_end();

// Start counting CPU ticks
tickStart  = rdtsc_begin();

    /* initialize fork processes */
    /* fork child processes */
    for (procCount = 0; procCount < _thread ;procCount++)
    {
        pid = fork ();
        if (pid < 0)              /* check for error      */
            printf ("Fork error.\n");
        else if (pid == 0)
	  // printf("Child process created. PID %d",(int)getpid());
	  break;
    }

   /********************************************************/
   /***********    Task of child processes   ***************/
   /********************************************************/

    if(pid==0) 
     {
       sem_wait (sem);      // Ask for semaphore resource
       printf ("\nChild process (#%d) is running.\n",(int) getpid());
        
       // The Warshall's algorithm
       while(kWar<_vertice)
	 {
	   {
	   while(*iWar<_vertice)
	     {
	       printf("Process #%d computing k=%d i=%d.\n",(int)getpid(),kWar,*iWar);
	       for (jWar=0; jWar < _vertice; jWar++)
		 {
		   matrix[*iWar][jWar] = matrix[*iWar][jWar] || (matrix[*iWar][kWar] && matrix[kWar][jWar]);
		 }

	       printf("\nProcess #%d done computering l=%d i=%d, semaphore released.\n\n",(int)getpid(),kWar,*iWar);
	       *iWar = *iWar +1;
	       sem_post (sem);      // Release semaphore resource
	       exit(0);

	     }
	
     }


   /********************************************************/
   /***********    Task of parent processes   **************/
   /********************************************************/

    if (pid != 0)
      {
        /* wait for all children to exit */
        while (pid = waitpid (-1, NULL, 0)){
            if (errno == ECHILD)
                break;
        }

        printf ("\nParent: All children have exited.\n");

	   //End counting CPU ticks
	tickEnd = rdtsc_end();
	tickEnd = tickEnd - tickStart;

	printf("cycles spent : %llu\n",tickEnd);
	printf("Time spent : %e (us)\n",(tickEnd)/2394.468);
	
	Print_graph();

        /* shared memory detach */
        shmdt (matrix);
        shmctl (shmid, IPC_RMID, 0);

        shmdt (iWar);
        shmctl (shmid_i, IPC_RMID, 0);

        /* cleanup semaphores */
        sem_destroy (sem);
	printf("\n\nSemaphore closed.\n\n");
        exit (0);
    }   

    return 0;

}


void Print_graph(void)
{

// Print out the transitive closure matrix for comparison
printf("\nTransitive closure graph matrix: \n");

 for(outX=0;outX<_vertice;outX++)
   {
     for(outY=0;outY<_vertice;outY++)
       {
	 printf("%d ",matrix[outX][outY]);
       }
     printf("\n");
   }

}






