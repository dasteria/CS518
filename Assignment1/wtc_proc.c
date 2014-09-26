#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
uint64_t rdtsc()
{
   uint32_t hi, lo;
   __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
   return (uint64_t)lo|((uint64_t)hi<<32);
}


int main()
{
  int ticks,warmup;
  int n,t1,t2;
  int inX,inY;
  int i,j,k;
  int outX,outY;


  FILE *my_file;
  char buf[] = "graph.in";
  my_file = fopen (buf, "r");
  fscanf (my_file, "%i", &n);
  int myGraph[n][n];  // Read number of vertices (first input)

 // Initialize matrix with 0

  for(inX=0;inX<n;inX++)
 {
    for(inY=0;inY<n;inY++)
    {
         myGraph[inX][inY] = 0 ;
    }
 }


 // Iterate through inputs and save 1 for each direct path
  while(fgets(buf,10,my_file)!=NULL)
{
  fscanf(my_file,"%i %i",&t1,&t2);
  myGraph[t1-1][t2-1] = 1;
}
  fclose (my_file);

// Print out input graph matrix for checking purpose
 printf("\nInitial graph matrix: \n");
  for(outX=0;outX<n;outX++)
 {
    for(outY=0;outY<n;outY++)
    {
         printf("%d ",myGraph[outX][outY]);
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
   for (k = 0; k < n; k++)
    {
        // Pick all vertices as source one by one
        for (i = 0; i < n; i++)
        {
            // Pick all vertices as destination for the
            // above picked source
            for (j = 0; j < n; j++)
            {
                // If vertex k is on a path from i to j,
                // then make sure that the value of reach[i][j] is 1
                myGraph[i][j] = myGraph[i][j] || (myGraph[i][k] && myGraph[k][j]);
            }
        }
    }

ticks = rdtsc()- ticks;

// Print out the transitive closure matrix for comparison
 printf("\nTransitive closure graph matrix: \n");
  for(outX=0;outX<n;outX++)
 {
    for(outY=0;outY<n;outY++)
    {
         printf("%d ",myGraph[outX][outY]);
    }
    printf("\n");
 }

printf ("\nTicks elapsed for Warshall's algorithm: %d\n", ticks);
printf ("Time elapsed: %e ns\n\n",(ticks*1000)/2394.468);

  return 0;
}