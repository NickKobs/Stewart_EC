// -------------------------------------------------------------------
// lab23a_Dining_Philosopher_Naive.cpp
//
//  to Compile:
//
//  g++ lab23a_Dining_Philosopher_Naive.cpp -lpthreads
//
//
// This is a simple version of dining philosopher where we 
// have possiblity of deadlock and livelock.
//
// NOTE:
// -----
// To make sure (or at least increase the chance of getting a 
// deadlock, see the commented lines in the Philosopher() thread.
// By sleeping right after we get our first fork, we can cause
// a deadlock.
//
//
// To avoid deadlock, we need to remove one of the 
// 4 necessary conditions of deadlock. (e.g., no preemption, 
// circular wait, etc..)

// To avoid livelock or starvation we need FAIRNESS!
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
#define N 5  					// number of philosophers

pthread_mutex_t forks[N];		// create 5 mutex for 5 forks

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

        pthread_mutex_lock(&forks[i]);				//get the left fork
        printf("Philosopher %d picked up LEFT fork %d\n", i, i);
	
		// This sleep is intentionally enabled for the submission
		// deadlock demonstration. It gives every philosopher enough
		// time to grab the left fork and then block forever waiting
		// for the right fork.
		sleep(1);

        pthread_mutex_lock(&forks[(i + 1) % N]);	//get the right fork
        printf("Philosopher %d picked up RIGHT fork %d\n", i, (i + 1) % N);

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

    // Join threads (never actually ends because phil thread is infinite loop!)
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
