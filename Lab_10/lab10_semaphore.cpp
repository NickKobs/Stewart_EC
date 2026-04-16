#include "lab10_semaphore.h"

#include "lab10_dashboard.h"

#include <sstream>
#include <stdexcept>

Semaphore::Semaphore(int initialValue, TaskDashboard& dashboard)
    : value_(initialValue), dashboard_(dashboard) {
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

    {
        std::ostringstream builder;
        builder << "Task " << thread_id << " requests a permit. Available before request: " << value_;
        dashboard_.logQueue(builder.str());
    }

    if (value_ == 0) {
        dashboard_.logTask(thread_id, "No permit available; joining the wait queue.");
        waitQueue_.En_Q(thread_id);
        dashboard_.logQueue("Queue state: " + waitQueue_.Get_Q_String());

        while (value_ == 0 || waitQueue_.Front() != thread_id) {
            dashboard_.logTask(thread_id, "Waiting for release signal.");
            pthread_cond_wait(&cond_, &lock_);
        }

        const int released_thread_id = waitQueue_.De_Q();
        dashboard_.logTask(
            released_thread_id,
            "Woke up, re-acquired the semaphore lock, and left the wait queue."
        );
        dashboard_.logQueue("Queue state: " + waitQueue_.Get_Q_String());
    }

    --value_;
    {
        std::ostringstream builder;
        builder << "Task " << thread_id << " acquired a permit. Available now: " << value_;
        dashboard_.logQueue(builder.str());
    }
    pthread_mutex_unlock(&lock_);
}

void Semaphore::Up(int thread_id) {
    pthread_mutex_lock(&lock_);

    ++value_;
    {
        std::ostringstream builder;
        builder << "Task " << thread_id << " released a permit. Available now: " << value_;
        dashboard_.logQueue(builder.str());
    }

    if (!waitQueue_.isEmpty()) {
        const int next_thread_id = waitQueue_.Front();
        dashboard_.sendTaskMessage(
            thread_id,
            next_thread_id,
            "I released the semaphore. Wake up and compete for the critical section."
        );
        dashboard_.logQueue("Queue state before signal: " + waitQueue_.Get_Q_String());
        pthread_cond_broadcast(&cond_);
    } else {
        dashboard_.logQueue("No tasks are waiting in the queue.");
    }

    pthread_mutex_unlock(&lock_);
}
