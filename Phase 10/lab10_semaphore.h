#ifndef EXTRA_CREDIT_LAB10_SEMAPHORE_H
#define EXTRA_CREDIT_LAB10_SEMAPHORE_H

#include <pthread.h>

#include "../queue.h"

class Semaphore {
public:
    explicit Semaphore(int initialValue = 1);
    ~Semaphore();

    void Down(int thread_id);
    void Up(int thread_id);

private:
    int value_;
    pthread_mutex_t lock_;
    pthread_cond_t cond_;
    Queue<int> waitQueue_;
};

#endif
