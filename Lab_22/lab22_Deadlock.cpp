#include <iostream>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t firstMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t secondMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sequenceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sequenceCondition = PTHREAD_COND_INITIALIZER;
int sequenceStep = 0;

void wait_for_step(const int expectedStep) {
    pthread_mutex_lock(&sequenceMutex);
    while (sequenceStep < expectedStep) {
        pthread_cond_wait(&sequenceCondition, &sequenceMutex);
    }
    pthread_mutex_unlock(&sequenceMutex);
}

void advance_to_step(const int nextStep) {
    pthread_mutex_lock(&sequenceMutex);
    sequenceStep = nextStep;
    pthread_cond_broadcast(&sequenceCondition);
    pthread_mutex_unlock(&sequenceMutex);
}

void* worker_thread_1(void* arg) {
    (void)arg;

    std::cout << "Task 1 starts:" << std::endl;
    advance_to_step(1);

    wait_for_step(2);

    pthread_mutex_lock(&firstMutex);
    std::cout << "Task 1 got resource 1" << std::endl;
    advance_to_step(3);

    sleep(1);

    wait_for_step(4);

    pthread_mutex_lock(&secondMutex);
    std::cout << "Task 1 got resource 2" << std::endl;

    pthread_mutex_unlock(&secondMutex);
    pthread_mutex_unlock(&firstMutex);
    return nullptr;
}

void* worker_thread_2(void* arg) {
    (void)arg;

    wait_for_step(1);

    std::cout << "Task 2 starts:" << std::endl;
    advance_to_step(2);

    wait_for_step(3);

    pthread_mutex_lock(&secondMutex);
    std::cout << "Task 2 got resource 2" << std::endl;
    advance_to_step(4);

    sleep(1);

    pthread_mutex_lock(&firstMutex);
    std::cout << "Task 2 got resource 1" << std::endl;

    pthread_mutex_unlock(&firstMutex);
    pthread_mutex_unlock(&secondMutex);
    return nullptr;
}

int main() {
    pthread_t task1;
    pthread_t task2;

    pthread_create(&task1, nullptr, worker_thread_1, nullptr);
    pthread_create(&task2, nullptr, worker_thread_2, nullptr);

    pthread_join(task1, nullptr);
    pthread_join(task2, nullptr);

    std::cout << "main() thread is ending" << std::endl;
    return 0;
}
