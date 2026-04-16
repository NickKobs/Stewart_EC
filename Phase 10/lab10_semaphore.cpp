#include "lab10_semaphore.h"

#include <cstdio>
#include <stdexcept>

Semaphore::Semaphore(int initialValue) : value_(initialValue) {
    if (initialValue < 0) {
        throw std::invalid_argument("semaphore initial value cannot be negative");
    }

    pthread_mutex_init(&lock_, nullptr);
    pthread_cond_init(&cond_, nullptr);
}

Semaphore::~Semaphore() {
    pthread_mutex_destroy(&lock_);
    pthread_cond_destroy(&cond_);
}

void Semaphore::Down(int thread_id) {
    pthread_mutex_lock(&lock_);

    if (value_ == 0) {
        std::printf("\tThread %d is being placed on queue\n", thread_id);
        waitQueue_.En_Q(thread_id);
        waitQueue_.Print();

        while (value_ == 0 || waitQueue_.Front() != thread_id) {
            std::printf("\tThread = %d waiting to be released from the queue\n", thread_id);
            pthread_cond_wait(&cond_, &lock_);
        }

        const int released_thread_id = waitQueue_.De_Q();
        std::printf("\tThread = %d just got released from the queue and re-acquired mutex lock\n",
                    released_thread_id);
    }

    --value_;
    pthread_mutex_unlock(&lock_);
}

void Semaphore::Up(int thread_id) {
    pthread_mutex_lock(&lock_);

    ++value_;
    std::printf("\tThread %d released the semaphore\n", thread_id);

    if (!waitQueue_.isEmpty()) {
        std::printf("\tBefore releasing thread from queue\n");
        waitQueue_.Print();
        std::printf("\tSignal blocked thread %d to be released\n", waitQueue_.Front());
        pthread_cond_broadcast(&cond_);
    }

    pthread_mutex_unlock(&lock_);
}
