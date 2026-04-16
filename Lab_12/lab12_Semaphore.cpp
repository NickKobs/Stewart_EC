#include "lab12_Semaphore.h"

#include "lab12_Scheduler.h"
#include "lab10_dashboard.h"

#include <stdexcept>
#include <utility>

Semaphore::Semaphore(std::string name, const int initial_value, TaskDashboard& dashboard)
    : name_(std::move(name)), value_(initial_value), dashboard_(dashboard) {
    if (initial_value < 0 || initial_value > 1) {
        throw std::invalid_argument("Lab 12 uses a binary semaphore, so the initial value must be 0 or 1");
    }
}

void Semaphore::attach_scheduler(Scheduler* sched_ptr) {
    sched_ptr_ = sched_ptr;
    dashboard_.logSystem("Semaphore " + name_ + " attached to the scheduler via sched_ptr.");
}

void Semaphore::down(const int task_id) {
    dashboard_.logSystem("Task " + std::to_string(task_id) + " called down() on " + name_ + '.');
    dashboard_.logTask(task_id, "Requesting semaphore " + name_ + '.');

    if (value_ > 0) {
        value_ = 0;
        owner_ = task_id;
        dashboard_.logTask(task_id, "Obtained semaphore " + name_ + '.');
        dashboard_.logSystem(
            "Semaphore " + name_ + " granted to task " + std::to_string(task_id) + '.'
        );
        return;
    }

    wait_queue_.En_Q(task_id);
    dashboard_.logTask(task_id, "Blocked and enqueued on " + name_ + '.');
    dashboard_.logSystem(
        "Semaphore " + name_ + " unavailable. Task " + std::to_string(task_id) + " joins the wait queue."
    );
    dashboard_.logQueue("Semaphore queue for " + name_ + ": " + wait_queue_.Get_Q_String());

    if (sched_ptr_ != nullptr) {
        sched_ptr_->block_task(task_id, "it is waiting on semaphore " + name_);
    }
}

void Semaphore::up(const int task_id) {
    dashboard_.logSystem("Task " + std::to_string(task_id) + " called up() on " + name_ + '.');

    if (owner_ != task_id) {
        dashboard_.logSystem(
            "up() ignored because task " + std::to_string(task_id) +
            " does not own semaphore " + name_ + '.'
        );
        return;
    }

    if (wait_queue_.IsEmpty()) {
        value_ = 1;
        owner_ = -1;
        dashboard_.logSystem(
            "Semaphore " + name_ + " released. No blocked task is waiting in the queue."
        );
        return;
    }

    const int next_owner = wait_queue_.De_Q();
    owner_ = next_owner;
    value_ = 0;
    dashboard_.logSystem(
        "Semaphore " + name_ + " handed off from task " + std::to_string(task_id) +
        " to task " + std::to_string(next_owner) + '.'
    );
    dashboard_.logQueue("Semaphore queue for " + name_ + ": " + wait_queue_.Get_Q_String());

    if (sched_ptr_ != nullptr) {
        sched_ptr_->unblock_task(
            next_owner,
            "task " + std::to_string(task_id) + " released semaphore " + name_
        );
    }
}

void Semaphore::dump(const std::string& title) const {
    dashboard_.logQueue("SEMAPHORE DUMP: " + title);
    dashboard_.logQueue(
        "name=" + name_ +
        " | value=" + std::to_string(value_) +
        " | owner=" + std::to_string(owner_) +
        " | wait_queue=" + wait_queue_.Get_Q_String()
    );
}
