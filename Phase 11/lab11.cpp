#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <string>
#include <time.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 3
#endif

#ifndef PRODUCE_COUNT
#define PRODUCE_COUNT 10
#endif

#ifndef PRODUCER_SLEEP_SEC
#define PRODUCER_SLEEP_SEC 0L
#endif

#ifndef PRODUCER_SLEEP_NSEC
#define PRODUCER_SLEEP_NSEC 100000000L
#endif

#ifndef CONSUMER_SLEEP_SEC
#define CONSUMER_SLEEP_SEC 0L
#endif

#ifndef CONSUMER_SLEEP_NSEC
#define CONSUMER_SLEEP_NSEC 850000000L
#endif

const bool verbose = false;
const long int nanoseconds = 1000000000L;

int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;
int buffer_count = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

void* producer(void* arg);
void* consumer(void* arg);
void simulate_IO_work(std::string taskName, long sec, long nsec);

int main() {
    pthread_t prod;
    pthread_t cons;

    if (verbose) {
        std::cout << "Parent: creates producer thread" << std::endl;
        std::cout << "Parent: creates consumer thread" << std::endl;
    }

    int create_result = pthread_create(&prod, nullptr, producer, nullptr);
    if (create_result != 0) {
        std::cerr << "pthread_create producer failed: " << strerror(create_result) << std::endl;
        return EXIT_FAILURE;
    }

    create_result = pthread_create(&cons, nullptr, consumer, nullptr);
    if (create_result != 0) {
        std::cerr << "pthread_create consumer failed: " << strerror(create_result) << std::endl;
        return EXIT_FAILURE;
    }

    if (verbose) {
        std::cout << "Parent waiting for producer & consumer to end" << std::endl;
    }

    int join_result = pthread_join(prod, nullptr);
    if (join_result != 0) {
        std::cerr << "pthread_join producer failed: " << strerror(join_result) << std::endl;
        return EXIT_FAILURE;
    }

    join_result = pthread_join(cons, nullptr);
    if (join_result != 0) {
        std::cerr << "pthread_join consumer failed: " << strerror(join_result) << std::endl;
        return EXIT_FAILURE;
    }

    if (verbose) {
        std::cout << "Destroy mutex and condition variables" << std::endl;
    }

    int destroy_result = pthread_mutex_destroy(&buffer_mutex);
    if (destroy_result != 0) {
        std::cerr << "pthread_mutex_destroy failed: " << strerror(destroy_result) << std::endl;
        return EXIT_FAILURE;
    }

    destroy_result = pthread_cond_destroy(&not_empty);
    if (destroy_result != 0) {
        std::cerr << "pthread_cond_destroy not_empty failed: " << strerror(destroy_result) << std::endl;
        return EXIT_FAILURE;
    }

    destroy_result = pthread_cond_destroy(&not_full);
    if (destroy_result != 0) {
        std::cerr << "pthread_cond_destroy not_full failed: " << strerror(destroy_result) << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void* producer(void* arg) {
    (void)arg;

    for (int i = 0; i < PRODUCE_COUNT; i++) {
        pthread_mutex_lock(&buffer_mutex);

        while (buffer_count == BUFFER_SIZE) {
            std::cout << "Producer waits... (Buffer is full)" << std::endl;
            pthread_cond_wait(&not_full, &buffer_mutex);
        }

        buffer[in] = i;
        printf("Produced: %d\n", i);

        in = (in + 1) % BUFFER_SIZE;
        buffer_count++;

        if (verbose) {
            std::cout << "Producer signals: (Buffer now has data)" << std::endl;
        }

        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&buffer_mutex);

        std::string taskName = "Producer";
        simulate_IO_work(taskName, PRODUCER_SLEEP_SEC, PRODUCER_SLEEP_NSEC);
    }

    return nullptr;
}

void* consumer(void* arg) {
    (void)arg;

    for (int i = 0; i < PRODUCE_COUNT; i++) {
        pthread_mutex_lock(&buffer_mutex);

        while (buffer_count == 0) {
            std::cout << "\tConsumer waits... (Buffer is empty)" << std::endl;
            pthread_cond_wait(&not_empty, &buffer_mutex);
        }

        int item = buffer[out];
        printf("\tConsumed: %d\n", item);

        out = (out + 1) % BUFFER_SIZE;
        buffer_count--;

        if (verbose) {
            std::cout << "\tConsumer signals: (Buffer now has empty space)" << std::endl;
        }

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&buffer_mutex);

        std::string taskName = "Consumer";
        simulate_IO_work(taskName, CONSUMER_SLEEP_SEC, CONSUMER_SLEEP_NSEC);
    }

    return nullptr;
}

void simulate_IO_work(std::string taskName, long sec, long nsec) {
    struct timespec req {};
    struct timespec rem {};

    req.tv_sec = sec;
    req.tv_nsec = nsec;

    if (verbose) {
        printf(
            "%s : sleep for (%.2f) seconds\n",
            taskName.c_str(),
            static_cast<double>(sec * nanoseconds + nsec) / nanoseconds
        );
    }

    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            req = rem;
        } else {
            perror("nanosleep");
            break;
        }
    }

    if (verbose) {
        printf("nanosleep() finished.\n");
    }
}
