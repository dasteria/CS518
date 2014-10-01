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

/*
* List of methods needed
*/

void Create_shm(void);	// Create shared memory to store matrix representation of the graph
void Ini_graph(int** mat); // Read input file, number of vertices and 2D transition matrix (change original matrix as a result)
void Comp_TranCls(int numVer, int** mat); // Take the 2D matrix and change it to the transitive closure matrix result
void Print_graph(int** mat);	// Output the graph to verify result


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

// Actually start timing
  tickStart = rdtsc();

// The Warshall's algorithm
  for (kWar = 0; kWar < _vertice; kWar++)
    {
// Pick all vertices as source one by one
      for (iWar = 0; iWar < _vertice; iWar++)
	{
// Pick all vertices as destination for the
// above picked source
	  for (jWar = 0; jWar < _vertice; jWar++)
	    {
// If vertex k is on a path from i to j,
// then make sure that the value of reach[i][j] is 1
 matrix[iWar][jWar] = matrix[iWar][jWar] || (matrix[iWar][kWar] && matrix[kWar][jWar]);
	    }
	}
    }

  tickEnd = rdtsc();
  tickEnd = tickEnd - tickStart;

 printf ("\nTicks elapsed for Warshall's algorithm: %llu\n", tickEnd);
 printf ("Time elapsed: %e us\n\n",(tickEnd)/2394.468);

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

 return 0;
}
