#ifndef EXTRA_CREDIT_LAB12_SEMAPHORE_H
#define EXTRA_CREDIT_LAB12_SEMAPHORE_H

#include <string>

#include "../queue.h"

class Scheduler;
class TaskDashboard;

class Semaphore {
public:
    Semaphore(std::string name, int initial_value, TaskDashboard& dashboard);

    void attach_scheduler(Scheduler* sched_ptr);
    void down(int task_id);
    void up(int task_id);
    void dump(const std::string& title) const;

private:
    std::string name_;
    int value_ = 1;
    int owner_ = -1;
    Queue<int> wait_queue_;
    Scheduler* sched_ptr_ = nullptr;
    TaskDashboard& dashboard_;
};

#endif
