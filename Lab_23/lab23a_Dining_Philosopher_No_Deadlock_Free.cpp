// -------------------------------------------------------------------
// lab23a_Dining_Philosopher_No_Deadlock_Free.cpp
//
//  to Compile:
//
//  g++ lab23a_Dining_Philosopher_No_Deadlock_Free.cpp -lpthreads
//
//
// This is a simple version of dining philosopher where we 
// avoid deadlocks.
//
// Avoid Deadlocks (but not necessarily livelock)
// ----------------------------------------------   
// We prevent deadlock by having one philosopher pick up forks 
// in reverse order which breaks "circular wait" condition
//
// We maintain 5 mutex, 1 for each fork
//
// All the threads except for the last thread pick up forks
// in the following order:
//
// 	P0 takes left → waits right
// 	P1 takes left → waits right
// 	  ..
// 	  ..
// 	P4 takes right → waits left    (note the difference)
//
// It is still possible to have starvation.
//
// To prevent starvation we need FAIRNESS!
//
// Starvation in operating systems is eliminated by ensuring 
// fair CPU and resource allocation, primarily through 
// aging—gradually increasing the priority of long-waiting 
// tasks—and using scheduling algorithms like Round Robin.
//
// These techniques prevent low-priority tasks from being
// perpetually delayed by high-priority tasks.
//
//
//  
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// -------------------------------------------------------------------
#define N 5  // number of philosophers

pthread_mutex_t forks[N];

// -------------------------------------------------------------------
// Utility function
void think(int i) {
    printf("Philosopher %d is thinking\n", i);
    sleep(1);
}

// -------------------------------------------------------------------
void eat(int i) {
    printf("\t Philosopher %d is eating\n", i);
    sleep(1);
}

// -------------------------------------------------------------------
// Philosopher thread
void* philosopher(void* arg) {
    int i = *(int*)arg;

    while (1) {
        think(i);

        // Deadlock avoidance:
		// note the order of forks being locked
        if (i == N - 1) {
            // Last philosopher picks right first
            pthread_mutex_lock(&forks[(i + 1) % N]);
            printf("Philosopher %d picked up RIGHT fork %d first\n", i, (i + 1) % N);
            pthread_mutex_lock(&forks[i]);
            printf("Philosopher %d picked up LEFT fork %d second\n", i, i);
        } else {
            pthread_mutex_lock(&forks[i]);
            printf("Philosopher %d picked up LEFT fork %d first\n", i, i);
            pthread_mutex_lock(&forks[(i + 1) % N]);
            printf("Philosopher %d picked up RIGHT fork %d second\n", i, (i + 1) % N);
        }

        eat(i);

        pthread_mutex_unlock(&forks[i]);
        pthread_mutex_unlock(&forks[(i + 1) % N]);
        printf("Philosopher %d put both forks down\n", i);
    }

    return NULL;
}

// -------------------------------------------------------------------
int main() {
    pthread_t threads[N];
    int ids[N];

    // Initialize forks (mutexes)
    for (int i = 0; i < N; i++) {
        pthread_mutex_init(&forks[i], NULL);
    }

    // Create philosopher threads
    for (int i = 0; i < N; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, philosopher, &ids[i]);
    }

    // Join threads (never actually ends)
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
