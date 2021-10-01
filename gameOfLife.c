#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

// Structs
typedef struct thread_data {
  int start;
  int stop;

  int grid_size;
  int time_steps;
  int num_of_threads;
  int simulation;

  pthread_t thread;
} thread_data_t;

// Global variables
short int** grid;
short int** temp_grid;

pthread_mutex_t lock;
pthread_cond_t cv;
int state = 0;
int waiting = 0;

// Functions
void initGrid(const int grid_size, short int** __restrict grid);
void* updateGrid(void* arg);
void printGrid(const int grid_size, short int** __restrict grid);
void barrier(const int num_of_threads, const int grid_size, const int simulation);

static double get_wall_seconds(){
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double seconds = tv.tv_sec + (double)tv.tv_usec / 1000000;
  return seconds;
}

int main(int argc, char* argv[]){
    const double time1 = get_wall_seconds(); // start time
    if (argc != 5){
        printf("Expected input: ./gameOfLife grid_size time_steps simulation num_of_threads\n");
        return -1;
    }
    const int grid_size = atoi(argv[1]);
    const int time_steps = atoi(argv[2]);
    const int simulation = atoi(argv[3]); // 1 = simulation on, 0 = simulation off
    if(simulation != 0 && simulation != 1){
        printf("Error: simulation input argument must be 1 or 0. \n");
        return -1;
    }
    const int num_of_threads = atoi(argv[4]);

    thread_data_t threads[num_of_threads];
    const int work_size = grid_size/num_of_threads;
    const int rest = grid_size%num_of_threads; // extra work amount for last thread

    // Initiate grids 
    short int** buffer_rows = (short int**)malloc((grid_size+grid_size+2)*sizeof(short int*));
    short int* buffer_col = (short int*)malloc((grid_size*grid_size+(grid_size+2)*(grid_size+2))*sizeof(short int*));

    grid = buffer_rows;
    for(int i = 0; i < grid_size; i++){
        grid[i] = (buffer_col + i*grid_size);
    }
    initGrid(grid_size, grid); // fill grid with start seed

    temp_grid = (buffer_rows+grid_size); // "old" (padded) grid
    for(int i = 0; i < grid_size+2; i++){
        temp_grid[i] = (buffer_col + grid_size*grid_size + i*(grid_size+2));
    }

    // Make deep copy of grid to temp_grid and add padding on edges
    for(int i = 1; i <= grid_size; i++){ // fill middle of grid
        for(int j = 1; j <= grid_size; j++){
            temp_grid[i][j] = grid[i-1][j-1];
        }
    }
    for(int j = 0; j < grid_size+2; j++){ // fill horizontal padding 
        temp_grid[0][j] = 0;
        temp_grid[grid_size+1][j] = 0;
    }
    for(int i = 1; i <= grid_size; i++){ // fill vertical padding
        temp_grid[i][0] = 0;
        temp_grid[i][grid_size+1] = 0;
    }

    // Initiate mutex and condition variables
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cv, NULL);

    // Create threads
    for(int k = 0; k < num_of_threads-1; k++){
        threads[k].start = (k+1) * work_size - work_size;
        threads[k].stop = threads[k].start + work_size;

        threads[k].grid_size = grid_size;
        threads[k].time_steps = time_steps;
        threads[k].num_of_threads = num_of_threads;
        threads[k].simulation = simulation;

        pthread_create(&(threads[k].thread), NULL, updateGrid, &threads[k]);
    }
    threads[num_of_threads-1].start = num_of_threads * work_size - work_size;
    threads[num_of_threads-1].stop = threads[num_of_threads-1].start + work_size + rest;
    threads[num_of_threads-1].grid_size = grid_size;
    threads[num_of_threads-1].time_steps = time_steps;
    threads[num_of_threads-1].num_of_threads = num_of_threads;
    threads[num_of_threads-1].simulation = simulation;
    pthread_create(&(threads[num_of_threads-1].thread), NULL, updateGrid, &threads[num_of_threads-1]);

    // Join threads
    for(int k = 0; k < num_of_threads; k++){
        pthread_join(threads[k].thread, NULL);
    }

    // Free allocated memory
    free(buffer_col);
    free(buffer_rows);

    // Destroy mutex and condition variables
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cv);

    printf("gameOfLife took %7.3f wall seconds.\n", get_wall_seconds()-time1);
    return 0;
}

void initGrid(const int grid_size, short int** __restrict grid){
    srand(1);
    for(int i = 0; i < grid_size; i++){
        for(int j = 0; j < grid_size; j++){
            grid[i][j] = rand()%2; // 0 = dead, 1 = alive
        }
    }
}

void* updateGrid(void* arg){
    thread_data_t* info = (thread_data_t *) arg;
    int neighbours;

    for(int k = 0; k < info->time_steps; k++){ // For every time step
        barrier(info->num_of_threads, info->grid_size, info->simulation);

        // Update temp_grid with most recent values
        for(int i = info->start+1; i <= info->stop; i++){ // fill middle of grid
            for(int j = 1; j <= info->grid_size; j++){
                temp_grid[i][j] = grid[i-1][j-1];
            }
        }

        // Compute new grid 
        for(int i = info->start+1; i <= info->stop; i++){ // specific rows
            for(int j = 1; j <= info->grid_size; j++){ // all columns
                neighbours = temp_grid[i-1][j-1]+temp_grid[i-1][j]+temp_grid[i-1][j+1]+
                temp_grid[i][j-1]+temp_grid[i][j+1]+temp_grid[i+1][j-1]+temp_grid[i+1][j]+
                temp_grid[i+1][j+1];

                if(temp_grid[i][j] == 0 && neighbours == 3){
                    grid[i-1][j-1] = 1;
                } else if(temp_grid[i][j] == 1 && (neighbours == 2 || neighbours == 3)){
                    grid[i-1][j-1] = 1;
                } else {
                    grid[i-1][j-1] = 0;
                }
            }
        }
    }
    return NULL;
}

void printGrid(const int grid_size, short int** __restrict grid){
    char c;
    for(int i = 0; i < grid_size; i++){
        for(int j = 0; j < grid_size; j++){
            if(grid[i][j] == 1){ // alive
                c = '*';
            } else { // dead
                c = '.';
            }
            printf("%c ", c);
        }
        printf("\n");
    }
}

void barrier(const int num_of_threads, const int grid_size, const int simulation){
    int mystate;
    pthread_mutex_lock(&lock);
    mystate = state;
    waiting++; // Number of threads ready for next time step
    
    if(waiting == num_of_threads){ // All threads ready for next timestep
        waiting = 0;
        state = 1-mystate;

        if(simulation == 1){ // simulation enabled
            printf("Current state:\n");
            printGrid(grid_size, grid); // Print grid
            nanosleep((const struct timespec[]){{0, 100000000L}}, NULL);
        }

        pthread_cond_broadcast(&cv); // Wake up sleeping threads
    }
    while(mystate == state){ // Not all threads are done
        pthread_cond_wait(&cv, &lock); // Let thread sleep
    }
    pthread_mutex_unlock(&lock);
}