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
int _vertice;	              /* Number of vertices for the graph we are calculating */
int _thread;                  /* Number of threads to compute in parallel */
int jWar;             /* Loop variables for Warshall's algorithm */

FILE *my_file;                /* Pointer to input file */
char buf[] = "graph.in";
int outX,outY;

key_t shmkey;                 /* key to be passed to shmget() */
int shmflg;                   /* shmflg to be passed to shmget() */
int shmid;                    /* return value from shmget() */
sem_t** sem_array;               /* Pointer to semphore array */

int matrixSize;               /* size to be passed to shmget() */
int** matrix;                 /* pointer to the matrix representation of graph */

key_t shmkey_i;
int shmflg_i;
int shmid_i;
int *iWar;                    /* Parallel computed loop variable in shared memory */

key_t shmkey_k;
int shmflg_k;
int shmid_k;
int *kWar;                    /* Parallel computed loop variable in shared memory */

key_t shmkey_sem;                 /* key to be passed to shmget() */
int shmflg_sem;                   /* shmflg to be passed to shmget() */
int shmid_sem;                    /* return value from shmget() */

sem_t *sem_issue_newI_parent;                   /*      synch semaphore         */
sem_t *sem_newI_child;
sem_t *sem_i_complete_parent;
sem_t *sem_share_complete_child;

/*
* List of methods needed
*/

void Create_shm(void);	// Create shared memory to store matrix representation of the graph
void Create_semaphore(void); // Create semaphores needed to compute
void Ini_graph(int** mat); // Read input file, number of vertices and 2D transition matrix
void Print_graph(void);	// Output the graph to verify result
void Clean_Up(void); // Delete and unattach shared memory and semaphores
void Create_Sem_Array(int n); // Create an array of semaphores for each process


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
  shmkey_k = ftok("/dev/bus",1);
  printf ("Matrix shmkey = %d\n\n", shmkey);
  printf ("i shmkey = %d\n\n",shmkey_i);
  printf("k shmkey = %d\n\n",shmkey_k);
  int size = numVertex * sizeof(int*) + numVertex*numVertex * sizeof(int*) + sizeof(int*);
  shmid = shmget (shmkey, size, 0666 | IPC_CREAT);
  shmid_i = shmget(shmkey_i,sizeof(int*),0666 | IPC_CREAT);
  shmid_k = shmget(shmkey_k,sizeof(int*),0666 | IPC_CREAT);
  // printf ("The matrix(%p) is allocated in shared memory.\n", matrix);
  if (shmid < 0 || shmid_i<0)
    {
      perror ("shmget\n");
      exit (1);
    }

  matrix = (int **) shmat (shmid, NULL, 0); /* attach matrix to shared memory */
  iWar = (int*) shmat(shmid_i,NULL,0);
  kWar = (int*) shmat(shmid_k,NULL,0);
  printf ("The matrix(%p) is allocated in shared memory.\n\n", matrix);
  printf ("The i loop variable(%p) is allocated in shared memory.\n\n",iWar);
  printf("The k loop variable (%p) is allocated in shared memory.\n\n",kWar);
}

void Create_Sem_Array(int n)
{
    int i;
    shmkey_sem = ftok ("/dev/cpu", 1);
    printf("Semaphore array shmkey = %d\n\n",shmkey_sem);

    int size = n * sizeof(sem_t*);
    shmid_sem = shmget(shmkey_sem,size,0666 | IPC_CREAT);
    sem_array = (sem_t**) shmat(shmid_sem,NULL,0);
    // int *head = (int*) sem_array;
    for(i=0;i<n;i++)
      {
	//sem_array[i] = head + i;
        sem_array[i] = sem_open ("Sem", O_CREAT | O_EXCL, 0644, 0); 
	sem_unlink("Sem");
	printf("The pointer for semaphore array[%d] is %p\n",i,sem_array[i]);
      }
    
}

void Delete_Sem_Array(int n)
{
  int i;
  for(i=0;i<n;i++)
    {
      	sem_destroy(sem_array[i]);
    }

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
  *kWar = 0;

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

        shmdt (iWar);
        shmctl (shmid_i, IPC_RMID, 0);

	shmdt(kWar);
	shmctl(shmid_k,IPC_RMID,0);
	printf("\nShared memory detached.\n");

        /* cleanup semaphores */
        sem_destroy (sem_issue_newI_parent);
	//	sem_destroy (sem_newI_child);
	//sem_destroy (sem_i_complete_parent);
	//sem_destroy (sem_share_complete_child);
	Delete_Sem_Array(_thread);
	shmdt(sem_array);
	shmctl(shmid_sem,IPC_RMID,0);
	printf("\nSemaphore closed.\n\n");
}

void Create_Semaphore(void)
{
     /* initialize semaphores for shared processes */
    sem_issue_newI_parent = sem_open ("Sem1", O_CREAT | O_EXCL, 0644, 1); 
    sem_unlink ("Sem1");      

    // sem_newI_child = sem_open ("Sem2", O_CREAT | O_EXCL, 0644, 0); 
    // sem_unlink ("Sem2");  

    //sem_i_complete_parent = sem_open ("Sem3", O_CREAT | O_EXCL, 0644, 0); 
    //sem_unlink ("Sem3");  

    //sem_share_complete_child = sem_open ("Sem4", O_CREAT | O_EXCL, 0644, 0); 
    //sem_unlink ("Sem4");  

    printf ("semaphores initialized. \n\n");

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

int main()
{
  pid_t pid;
  unsigned long long tickStart,tickEnd;
  Create_shm();
  Create_Semaphore();
  Create_Sem_Array(_thread);
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

 int childStart,childEnd,myNumber;
    /* initialize fork processes */
    /* fork child processes */
    for (procCount = 0; procCount < _thread ;procCount++)
    {
        pid = fork ();
        if (pid < 0)              /* check for error      */
            printf ("Fork error.\n");
        else if (pid == 0)
	  {
	    // Each child has its own share of i interval
	    myNumber = procCount;
	    childStart =((_vertice + _thread - 1)/_thread) * procCount;
	    childEnd = childStart + ((_vertice + _thread - 1)/_thread) -1 ; 
	    if(childEnd >= _vertice)
	      childEnd = _vertice -1 ;
	    break;
	  }
	  // printf("Child process created. PID %d",(int)getpid());
	  
    }

    /*****************************************************/
    /*****************  Parent Process *******************/
    /*****************************************************/
    if(pid !=0)
      {
	while(*kWar != -1)
	  {
	// Wait for permission to start new round of K
	sem_wait(sem_issue_newI_parent);
	int threadNum;
	int recover;
	for(threadNum = 0; threadNum<_thread;threadNum++)
	  {
	    sem_post(sem_array[threadNum]);
	    //printf("\nGiving children command to start.\n");
	  }
	
	while(1)
	  {
	    if(*iWar == _thread)
	      break;
	  }
	    printf("\nAll processes for current k finished.");	

	    if(*kWar ==  _vertice-1)
	      {
		*kWar = -1;
		break;
	      }
	    else
	      {
	    *kWar = *kWar + 1;
	    // printf("\nNext k value: %d",*kWar);
	    *iWar = 0;
	    sem_post(sem_issue_newI_parent);
	      }

	  }

	
	 //End counting CPU ticks
	tickEnd = rdtsc_end();
	tickEnd = tickEnd - tickStart;

	printf("\nCycles spent : %llu\n",tickEnd);
	printf("\nTime spent : %e (us)\n",(tickEnd)/2394.468);
	printf("\nTransitive Closure graph output:");
	Print_graph();
	Clean_Up();
	return 0;
	
      }
    

    /*****************************************************/
    /*****************  Child Process ********************/
    /*****************************************************/
    if(pid == 0)
      {
	while(*kWar != -1)
	  {
	sem_wait(sem_array[myNumber]);
	//printf("\nChild semaphone on, start computing.\n");
	for(;childStart<=childEnd;childStart++)
	  {
	    printf("\nStart computing k=%d, i=%d\n",*kWar,childStart);

	    for (jWar = 0; jWar < _vertice; jWar++)
	      {
 matrix[childStart][jWar] = matrix[childStart][jWar] || (matrix[childStart][*kWar] && matrix[*kWar][jWar]);
	      }
	  }
	childStart = childEnd - (((_vertice + _thread - 1)/_thread) -1);
	*iWar = *iWar + 1;
	//sem_post(sem_array[myNumber]);
	  }

	exit(0);

      }
	  

}   










