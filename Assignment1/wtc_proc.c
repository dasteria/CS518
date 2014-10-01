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
int iWar,jWar,kWar;           /* Loop variables for Warshall's algorithm */
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
pid_t pid;


/*
* List of methods needed
*/

void Create_shm(void);	// Create shared memory to store matrix representation of the graph
void Ini_graph(int** mat); // Read input file, number of vertices and 2D transition matrix
void Comp_TranCls(int numVer, int** mat); // Take the 2D matrix and change it to the transitive closure matrix result
void Print_graph(void);	// Output the graph to verify result

static __inline__ unsigned long long rdtsc(void)
{
  unsigned cycles_high,cycles_low,cycles_high1,cycles_low1;

 __asm__ __volatile__
(

"CPUID\n\t"/*serialize*/
"RDTSC\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t": "=r" (cycles_high), "=r"
(cycles_low):: "%rax", "%rbx", "%rcx", "%rdx"

); 

/*
Call the function to benchmark
*/

 __asm__ __volatile__
(

"RDTSCP\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t"
"CPUID\n\t": "=r" (cycles_high1), "=r"
(cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx"

);

 __asm__ __volatile__
(

"CPUID\n\t"/*serialize*/
"RDTSC\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t": "=r" (cycles_high), "=r"
(cycles_low):: "%rax", "%rbx", "%rcx", "%rdx"

); 

/*
Call the function to benchmark
*/

 __asm__ __volatile__
(

"RDTSCP\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t"
"CPUID\n\t": "=r" (cycles_high1), "=r"
(cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx"

);

 __asm__ __volatile__
(

"CPUID\n\t"/*serialize*/
"RDTSC\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t": "=r" (cycles_high), "=r"
(cycles_low):: "%rax", "%rbx", "%rcx", "%rdx"

); 

/*
Call the function to benchmark
*/

 __asm__ __volatile__
(

"RDTSCP\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t"
"CPUID\n\t": "=r" (cycles_high1), "=r"
(cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx"

);

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
  printf ("shmkey = %d\n", shmkey);
  int size = numVertex * sizeof(int*) + numVertex*numVertex * sizeof(int*);
  shmid = shmget (shmkey, size, 0666 | IPC_CREAT);
  printf ("The matrix(%p) is allocated in shared memory.\n", matrix);
  
  if (shmid < 0)
    {
      perror ("shmget\n");
      exit (1);
    }

  matrix = (int **) shmat (shmid, NULL, 0); /* attach matrix to shared memory */
  printf ("The matrix(%p) is allocated in shared memory.\n", matrix);
}


void Ini_graph(int** matrix)
{
  int x,y,start,end;
  int *head = (int*)(matrix+_vertice);
  int i;
  for(i=0;i<_vertice;i++)
    {
      matrix[i] = head + i*_vertice;
      printf("The pointer for i(%d) is %p \n",i,matrix[i]);
    }

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

printf("Matrix initialization complete.\n");


// Iterate through inputs and save 1 for each direct path
 while(fgets(buf,10,my_file)!=NULL)
   {
     fscanf(my_file,"%i %i",&start,&end);
     printf("This pair of input is %i %i \n",start,end);
     matrix[start-1][end-1] = 1;
   }
     fclose (my_file);
}



int main()
{
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
    sem = sem_open ("pSem", O_CREAT | O_EXCL, 0666, 100); 
    /* name of semaphore is "pSem", semaphore is reached using this name */
    sem_unlink ("pSem");      
    /* unlink prevents the semaphore existing forever */
    /* if a crash occurs during the execution         */
    printf ("\n\nsemaphores initialized.\n\n");


// Warmup for RDSTC

/*
  for(warmup=0;warmup<100;warmup++)
    {
      ticks = rdtsc();
    }
*/

// Actually start timing
//  ticks = rdtsc();



    /* initialize fork processes  */
      for(procCount=0;procCount<_thread;procCount++)
	{
	  pid = fork();
	  if(pid<0)
	    printf("Fork error\n");
	  else if(pid==0)
	    break;
	}


  
tickStart  = rdtsc();



    /****************************************************/
    /************  Task of parent process ***************/
    /****************************************************/


      if (pid != 0)
     {
        /* wait for all children to exit */
        while (pid = waitpid (-1, NULL, 0))
	{
            if (errno == ECHILD)
                break;
        }
	tickEnd    = rdtsc();
tickEnd = tickEnd - tickStart;

printf("cycles spent : %llu\n",tickEnd);
printf("Time spent : %llu (ms)\n",(tickEnd)/2394468);

        printf ("\nParent: All children have exited.\n");
	// ticks = rdtsc()- ticks;
	Print_graph();

        /* shared memory detach */
        shmdt (matrix);
        shmctl (shmid, IPC_RMID, 0);

        /* cleanup semaphores */
        sem_destroy (sem);
        exit (0);
    } 


   /********************************************************/
   /***********    Task of child processes   ***************/
   /********************************************************/

    else
    {
        sem_wait (sem);           /* P operation */
        printf ("  Child(%d) is in critical section.\n", procCount);
        sleep (1);
        
        // The Warshall's algorithm
  for (kWar = 0; kWar < _vertice; kWar++)
    {
      for(iWar=0;iWar<_vertice;iWar++)
	  {
	     for (jWar = 0; jWar < _vertice; jWar++)
		 {
		     matrix[iWar][jWar] = matrix[iWar][jWar] || (matrix[iWar][kWar] && matrix[kWar][jWar]);
		 }
	  }
	    
	
    }
        sem_post (sem);           /* V operation */
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
 // printf ("\nTicks elapsed for Warshall's algorithm: %d\n", ticks);
 // printf ("Time elapsed: %e ns\n\n",(ticks*1000)/2394.468);

}


/*
uint64_t rdtsc()
{
  uint32_t hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return (uint64_t)lo|((uint64_t)hi<<32);
}
*/


