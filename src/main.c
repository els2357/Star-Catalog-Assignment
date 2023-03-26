// MIT License
// 
// Copyright (c) 2023 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <pthread.h>
#include "utility.h"
#include "star.h"
#include "float.h"

#define NUM_STARS 30000 
#define MAX_LINE 1024
#define DELIMITER " \t\n"

struct Star star_array[ NUM_STARS ];

struct t_arg {
    int id; //thread id
    int numthreads; //number of threads
    struct Star *sa; //pointer to star array struct
};

uint8_t   (*distance_calculated)[NUM_STARS];

double  min  = FLT_MAX;
double  max  = FLT_MIN;
int threads = 1;
double mean = 0;
uint64_t count = 0;

pthread_mutex_t mutex;

void showHelp()
{
  printf("Use: findAngular [options]\n");
  printf("Where options are:\n");
  printf("-t          Number of threads to use\n");
  printf("-h          Show this help\n");
}

void *determineAverageAngularDistance( void *args )
{
    struct t_arg *myargs = (struct t_arg *)args;
    int myid = myargs->id;
    struct Star *mystar = myargs->sa;

    uint32_t i, j;

    int num_sections = NUM_STARS/threads;
    int start = num_sections * myid;
    int end = num_sections * (myid + 1);

    pthread_mutex_lock(&mutex);
    for (i = start; i < end; i++)
    {
      for (j = 0; j < NUM_STARS; j++)
      {
        if( i!=j && distance_calculated[i][j] == 0 )
        {
          double distance = calculateAngularDistance( mystar[i].RightAscension, mystar[j].Declination,
                                                      mystar[j].RightAscension, mystar[j].Declination ) ;
          distance_calculated[i][j] = 1;
          distance_calculated[j][i] = 1;
          count++;

          if( min > distance )
          {
            min = distance;
          }

          if( max < distance )
          {
            max = distance;
          }
          mean = mean + (distance-mean)/count;
        }
      }
    }
    pthread_mutex_unlock(&mutex);
}


int main( int argc, char * argv[] )
{
  FILE *fp;
  uint32_t star_count = 0;
  uint32_t n;

  distance_calculated = malloc(sizeof(uint8_t[NUM_STARS][NUM_STARS]));

  if( distance_calculated == NULL )
  {
    uint64_t num_stars = NUM_STARS;
    uint64_t size = num_stars * num_stars * sizeof(uint8_t);
    printf("Could not allocate %ld bytes\n", size);
    exit( EXIT_FAILURE );
  }

  int i, j;
  // default every thing to 0 so we calculated the distance.
  // This is really inefficient and should be replace by a memset
  for (i = 0; i < NUM_STARS; i++)
  {
    for (j = 0; j < NUM_STARS; j++)
    {
      distance_calculated[i][j] = 0;
    }
  }

for( n = 1; n < argc; n++ )          
  {
    if( strcmp(argv[n], "-t" ) == 0 )
    {
      printf("Enter the number of threads: ");
      scanf("%d", &threads);
      printf("The number you entered was %d \n", threads);
    }
  }

  for( n = 1; n < argc; n++ )          
  {
    if( strcmp(argv[n], "-help" ) == 0 )
    {
      showHelp();
      exit(0);
    }
  }

  fp = fopen( "data/tycho-trimmed.csv", "r" );

  if( fp == NULL )
  {
    printf("ERROR: Unable to open the file data/tycho-trimmed.csv\n");
    exit(1);
  }

  char line[MAX_LINE];
  while (fgets(line, 1024, fp))
  {
    uint32_t column = 0;

    char* tok;
    for (tok = strtok(line, " ");
            tok && *tok;
            tok = strtok(NULL, " "))
    {
       switch( column )
       {
          case 0:
              star_array[star_count].ID = atoi(tok);
              break;
       
          case 1:
              star_array[star_count].RightAscension = atof(tok);
              break;
       
          case 2:
              star_array[star_count].Declination = atof(tok);
              break;

          default: 
             printf("ERROR: line %d had more than 3 columns\n", star_count );
             exit(1);
             break;
       }
       column++;
    }
    star_count++;
  }
  printf("%d records read\n", star_count );
  
  int valid = 0;
  pthread_t *thread_array;
  thread_array = malloc(threads * sizeof(pthread_t)); 
  
  struct t_arg *thread_args = malloc(threads * sizeof(struct t_arg));
  if (!thread_array || !thread_args) printf("ERROR: malloc failed!");

//fill thread array with parameters
  for (int t = 0; t < threads; t++) {
        thread_args[t].id = t;
        thread_args[t].numthreads = threads;
        thread_args[t].sa = star_array; //pointer to array
  }

  valid = pthread_mutex_init(&mutex, NULL); //initialize the mutex
  if (valid) printf("ERROR: pthread_mutex_init failed");
  
  for (int t = 0; t < threads; t++) {
    valid = pthread_create( &thread_array[t], NULL, determineAverageAngularDistance, &thread_args[t]);
    if (valid) printf("ERROR: pthread_create failed");
  }
  
  for (int t = 0; t < threads; t++) {
    valid = pthread_join(thread_array[t], NULL);
    if (valid) printf("ERROR: pthread_join failed");
  }
  
  pthread_mutex_destroy(&mutex); //destroy (free) the mutex
  free(thread_array);

  double distance = 0;
  //double distance =  determineAverageAngularDistance( star_array );
  printf("Average distance found is %lf\n", mean );
  printf("Minimum distance found is %lf\n", min );
  printf("Maximum distance found is %lf\n", max );

  return 0;
}

