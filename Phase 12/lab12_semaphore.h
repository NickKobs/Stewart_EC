#ifndef EXTRA_CREDIT_LAB12_SEMAPHORE_H
#define EXTRA_CREDIT_LAB12_SEMAPHORE_H

#include <string>

#include "lab12_scheduler.h"
#include "../queue.h"

class semaphore {
public:
    semaphore(int starting_value, std::string name, scheduler* theScheduler);
    ~semaphore() = default;

    void down(int taskID);
    void up();
    void dump(int level) const;

private:
    void publish_state() const;
    void emit_system(const std::string& message) const;
    void emit_task(int taskId, const std::string& message) const;

    std::string resource_name_;
    int sema_value_;
    int lucky_task_;
    Queue<int> sema_queue_;
    scheduler* sched_ptr_;
};

#endif
