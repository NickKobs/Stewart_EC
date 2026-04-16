//  lab23c_Dining_Philosopher_Starvation_Free.cpp
//
//  to Compile:
//
//  g++ lab23c_Dining_Philosopher_Starvation_Free.cpp -lpthreads
//
//
//
//	Each philosopher has a state:
//
//		THINKING
//		HUNGRY
//		EATING
//
//	A philosopher eats only if neighbors are NOT eating
//	When done → signals neighbors
// 
// Solves deadlock and starvation for the purposes of this lab
//

//-------------------------------------------------------------------
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
//-------------------------------------------------------------------

#define N 5      // number of philosophers/forks

typedef enum { THINKING, HUNGRY, EATING } state_t;

state_t state[N];
pthread_mutex_t mutex;
pthread_cond_t cond[N];

//-------------------------------------------------------------------
// Helper functions:
//
int left(int i)  { return (i + N - 1) % N; }	// left philosoher (circular)
int right(int i) { return (i + 1) % N; }		// right philosopher (circular)

//-------------------------------------------------------------------
// Thinking philosopher sleep for a second!
//
void think(int i) {
    printf("Philosopher %d is THINKING\n", i);
    sleep(1);
}

//-------------------------------------------------------------------
// Eating philosophers eat for a second
//
void eat(int i) {
    printf("\tPhilosopher %d is EATING\n", i);
    sleep(1);
}

//-------------------------------------------------------------------
// Check if philosopher can eat
// Check if the philosoper is HUNGRY and its Neighbors are not EATING
//
void test(int i) {
    if (state[i] == HUNGRY &&
        state[left(i)] != EATING &&
        state[right(i)] != EATING) {

        state[i] = EATING;
        pthread_cond_signal(&cond[i]);    // wakeup the philosopher thread if sleep
    }
    // if the above IF statement was not true
    // One of the Neighbors was EATING!
    // so we go back!  Hopefully, when they are finished
    // EATING, they will put the fork down and that 
    // causes our state to be changed to EATING!
}
//-------------------------------------------------------------------
// Pick up forks
void take_forks(int i) {
    pthread_mutex_lock(&mutex);

    state[i] = HUNGRY;
    printf("Philosopher %d is HUNGRY\n", i);

    test(i);   // this will either change your state to EATING or it comes back right away
	       // without changing state.

    // Wait if not able to eat
    //
    // if we came back from test(i) successfully, 
    // 		we are good to go,
    //		we can start eating.
    // else 
    // 		we have to cond_wait() until our state is changed to EATING
    while (state[i] != EATING) {
        pthread_cond_wait(&cond[i], &mutex);
    }


// Question?
// ---------
// Should we change our state to EATING?
// Who is changing our state to EATING?
//
// Hint: look at put_forks() and test() below.

    pthread_mutex_unlock(&mutex);
}

// -----------------------------------------------------------------
// Put down forks
void put_forks(int i) {
    pthread_mutex_lock(&mutex);

    state[i] = THINKING;

    // Let neighbors try
    test(left(i));
    test(right(i));

    pthread_mutex_unlock(&mutex);
}

// -----------------------------------------------------------------
// Philosopher thread
void* philosopher(void* arg) {
    int i = *(int*)arg;

    while (1) {
        think(i);
        take_forks(i);
        eat(i);
        put_forks(i);
    }
    return NULL;
}

// -----------------------------------------------------------------
int main() {
    pthread_t threads[N];
    int ids[N];

    pthread_mutex_init(&mutex, NULL);

    for (int i = 0; i < N; i++) {
        pthread_cond_init(&cond[i], NULL);
        state[i] = THINKING;
    }

    for (int i = 0; i < N; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, philosopher, &ids[i]);
    }

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
