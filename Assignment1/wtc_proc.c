#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int ticks,warmup;   /* 		Variable for measuring CPU time 				 	*/
int _vertice;		/*      Number of vertices for the graph we are calculating */
int _thread;		/* 		Number of threads to compute in parallel			*/
int i,j,k;          /* 		Loop variables for Warshall's algorithm 		 	*/
FILE *my_file;      /* 		Pointer to input file 								*/
char buf[] = "graph.in";
int outX,outY;

key_t shmkey;  	  /*  key to be passed to shmget()      */ 
int shmflg; 	  /*  shmflg to be passed to shmget()   */ 
int shmid;  	  /*  return value from shmget()        */ 
int matrixSize;   /*  size to be passed to shmget()     */ 
int** matrix;     /*  pointer to the matrix representation of graph */


/*
 * List of methods needed
 */
 
void Create_shm(void);							// Create shared memory to store matrix representation of the graph
void Ini_graph(int** mat);  						// Read input file, number of vertices and 2D transition matrix (change original matrix as a result)
void Comp_TranCls(int numVer, int** matrix);    	// Take the 2D matrix and change it to the transitive closure matrix result
void Print_graph(int** matrix);						// Output the graph to verify result
uint64_t rdtsc();									// Compute CPU ticks

void Create_shm(void)
{
	int numThread,numVertex;
	my_file = fopen (buf, "r");
	fscanf (my_file, "%i", &numThread);
	fscanf (my_file, "%i", &numVertex);
	printf("\nNumber of threads: %i\n",numThread);
	printf("Number of vertice: %i\n",numVertex);
	
	_vertice = numVertex;
	
	shmkey = ftok ("/dev", 1);  
	printf ("shmkey = %d\n", shmkey);
	
	int size = numVertex * sizeof(int*) + numVertex*numVertex * sizeof(int*);
	
    shmid = shmget (shmkey, size, 0666 | IPC_CREAT);
    if (shmid < 0){                           /* shared memory error check */
        perror ("shmget\n");
        exit (1);
    }
    
    matrix = (int **) shmat (shmid, NULL, 0);   /* attach matrix to shared memory */
    printf ("The matrix(%p) is allocated in shared memory.\n", matrix);
}

void Ini_graph(int** matrix)
{
	int x,y,start,end;
	int *head  = (int*)(matrix+_vertice);
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
			matrix[x][y] = 0 ;
		}
	}
	
	printf("Matrix initialization complete.\n");
	
	// Iterate through inputs and save 1 for each direct path
	while(fgets(buf,10,my_file)!=NULL)
	{
		fscanf(my_file,"%i %i",&start,&end);
		printf("This pair of input is %i  %i \n",start,end);
		matrix[start-1][end-1] = 1;
	}

	fclose (my_file);

}


int main()
{

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

// Warmup for RDSTC
for(warmup=0;warmup<100;warmup++)
{
	ticks = rdtsc();
}
// Actually start timing
ticks = rdtsc();

// The Warshall's algorithm
for (k = 0; k < _vertice; k++)
{
// Pick all vertices as source one by one
	for (i = 0; i < _vertice; i++)
	{
	// Pick all vertices as destination for the
	// above picked source
		for (j = 0; j < _vertice; j++)
		{
			// If vertex k is on a path from i to j,
			// then make sure that the value of reach[i][j] is 1
			matrix[i][j] = matrix[i][j] || (matrix[i][k] && matrix[k][j]);
		}
	}
}

ticks = rdtsc()- ticks;
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

printf ("\nTicks elapsed for Warshall's algorithm: %d\n", ticks);
printf ("Time elapsed: %e ns\n\n",(ticks*1000)/2394.468);

return 0;
}


uint64_t rdtsc()
{
uint32_t hi, lo;
__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
return (uint64_t)lo|((uint64_t)hi<<32);
}
