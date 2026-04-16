#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "lab10_semaphore.h"

#ifndef THREAD_COUNT
#define THREAD_COUNT 3
#endif

#ifndef SEMAPHORE_PERMITS
#define SEMAPHORE_PERMITS 1
#endif

#ifndef CRITICAL_SECTION_ROUNDS
#define CRITICAL_SECTION_ROUNDS 4
#endif

void* worker(void* arg);

Semaphore sem(SEMAPHORE_PERMITS);

int main() {
    pthread_t threads[THREAD_COUNT];
    long thread_ids[THREAD_COUNT];

    std::printf("Creating %d child threads\n", THREAD_COUNT);
    for (long i = 0; i < THREAD_COUNT; ++i) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], nullptr, worker, &thread_ids[i]) != 0) {
            std::fprintf(stderr, "Failed to create thread %ld\n", i);
            return 1;
        }
    }

    std::printf("Parent waiting for child threads to end.\n");
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(threads[i], nullptr);
    }

    return 0;
}

void* worker(void* arg) {
    const auto id = static_cast<int>(*static_cast<long*>(arg));

    sem.Down(id);
    std::printf("\tThread %d ENTER critical section\n", id);

    for (int i = 0; i < CRITICAL_SECTION_ROUNDS; ++i) {
        std::printf("I am thread # %d\n", id);
        sleep(1);
    }

    std::printf("\tThread %d EXIT critical section\n", id);
    sem.Up(id);
    std::printf("\t----------------------------------------------\n");

    return nullptr;
}
