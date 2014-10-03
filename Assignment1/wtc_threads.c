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

int procCount;                 /* Count of # of threads */
int _vertice;	              /* Number of vertices for the graph we are calculating */
int _thread;                  /* Number of threads to compute in parallel */
int jWar;             /* Loop variables for Warshall's algorithm */
int final_outX,final_outY;

FILE *my_file;                /* Pointer to input file */
char buf[] = "graph.in";
int outX,outY;

key_t shmkey;                 /* key to be passed to shmget() */
int shmflg;                   /* shmflg to be passed to shmget() */
int shmid;                    /* return value from shmget() */

int matrixSize;               /* size to be passed to shmget() */
int** matrix;                 /* pointer to the matrix representation of graph */

int iWar;                    /* Parallel computed loop variable in shared memory */
int kWar;                    /* Parallel computed loop variable in shared memory */

pthread_t *tid;              /* The thread identifier */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t init = PTHREAD_COND_INITIALIZER;

/*
* List of methods needed
*/

void Create_shm(void);	// Create shared memory to store matrix representation of the graph
void Create_semaphore(void); // Create semaphores needed to compute
void Ini_graph(int** mat); // Read input file, number of vertices and 2D transition matrix
void Print_graph(void);	// Output the graph to verify result
void Clean_Up(void); // Delete and unattach shared memory and semaphores
void Create_Sem_Array(int n); // Create an array of semaphores for each process
void *mythread(void *arg);
void Final_Output(void);


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

  printf ("Matrix shmkey = %d\n\n", shmkey);

  int size = numVertex * sizeof(int*) + numVertex*numVertex * sizeof(int*) + sizeof(int*);
  shmid = shmget (shmkey, size, 0666 | IPC_CREAT);

  // printf ("The matrix(%p) is allocated in shared memory.\n", matrix);
  if (shmid < 0)
    {
      perror ("shmget\n");
      exit (1);
    }

  matrix = (int **) shmat (shmid, NULL, 0); /* attach matrix to shared memory */

  printf ("The matrix(%p) is allocated in shared memory.\n\n", matrix);

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

  iWar = 0;
  kWar = 0;

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

void Clean_Up(void)
{
         /* shared memory detach */
        shmdt (matrix);
        shmctl (shmid, IPC_RMID, 0);

	printf("\nShared memory detached.\n");

}



void Print_graph(void)
{

// Print out the transitive closure matrix for comparison
printf("\nMatrix representation of graph: \n");

 for(outX=0;outX<_vertice;outX++)
   {
     for(outY=0;outY<_vertice;outY++)
       {
	 printf("%d ",matrix[outX][outY]);
       }
     printf("\n");
   }

}

/************************************************************/
/************** Child Thread Behavior ***********************/
/************************************************************/

void *mythread(void *arg)
{
  int threadStart = (intptr_t) arg;
  // compute Warshall's algorithm for assigned i intervals
  // k is fixed and coordinated by main thread
  // i is passed in as argument
  // j is looped through

  int threadEnd = threadStart + ((_vertice + _thread - 1)/_thread) -1 ; 
      if(threadEnd >= _vertice)
	  threadEnd = _vertice -1 ;
      pthread_mutex_lock(&lock);
	for(;threadStart<=threadEnd;threadStart++)
	  {
	    printf("\nStart computing k=%d, i=%d\n",kWar,threadStart);

	    for (jWar = 0; jWar < _vertice; jWar++)
	      {
 matrix[threadStart][jWar] = matrix[threadStart][jWar] || (matrix[threadStart][kWar] && matrix[kWar][jWar]);
	      }
	  }

	pthread_mutex_unlock(&lock);
        threadStart = threadEnd - (((_vertice + _thread - 1)/_thread) -1);
	iWar = iWar +1;
	
}

void Final_Output(void)
{
  printf("%d\n",_thread);
  printf("%d\n",_vertice);
  for(final_outX=0;final_outX<_vertice;final_outX++)
    {
      for(final_outY=0;final_outY<_vertice;final_outY++)
	{
	  if(matrix[final_outX][final_outY] == 1)
	    printf("%d %d\n",final_outX,final_outY);
	  else
	    continue;
	}
    }
}




int main()
{
  unsigned long long tickStart,tickEnd;
  Create_shm();
  Ini_graph(matrix);
  // Print out input graph matrix for checking purpose
  printf("\nInitial graph matrix: \n");
  Print_graph();

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


// Initialize the mutex lock
pthread_mutex_init(&lock, NULL);

tid = malloc(_thread * sizeof(pthread_t)); 

 int childStart,childEnd;
 int loopCount;

 while(1)
   {
    for (procCount = 0; procCount < _thread ;procCount++)
    {
      pthread_mutex_lock(&lock); // Lock the global variable so no other threads can access it
      // Each child has its own share of i interval
      childStart =((_vertice + _thread - 1)/_thread) * procCount;


      pthread_create(&tid[procCount], NULL, mythread, (void *) (intptr_t)childStart);
      pthread_mutex_unlock(&lock); // Unlock the global variable so other threads can access it.

    }

    // Wait for threads to finish
    for(loopCount=0; loopCount < _thread; loopCount++)
    {
            pthread_join(tid[loopCount], NULL);
    }
    printf("All threads finished for k=%d",kWar);

    if(kWar != _vertice)
      kWar= kWar +1;
    else
      break;
   }

    // Clean up the mutex when we are done with it.
    pthread_mutex_destroy(&lock);
	
	 //End counting CPU ticks
	tickEnd = rdtsc_end();
	tickEnd = tickEnd - tickStart;

	printf("\nTransitive Closure graph output:\n\n");
	Print_graph();
	printf("\nExpected output format:");
	Final_Output();
	printf("\nCycles spent : %llu\n",tickEnd);
	printf("\nTime spent : %e (us)\n",(tickEnd)/2394.468);
	Clean_Up();
	return 0;
	  

}   










