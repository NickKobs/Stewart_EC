#include "lab12_semaphore.h"

#include <iostream>
#include <utility>

semaphore::semaphore(const int starting_value, std::string name, scheduler* theScheduler)
    : resource_name_(std::move(name)),
      sema_value_(starting_value),
      lucky_task_(-1),
      sched_ptr_(theScheduler) {}

void semaphore::down(const int taskID) {
    if (taskID == lucky_task_) {
        std::cout << "Task # " << lucky_task_
                  << " already has the resource!  Ignore request." << std::endl;
        dump(1);
        return;
    }

    if (sema_value_ >= 1) {
        --sema_value_;
        lucky_task_ = taskID;
        dump(1);
        return;
    }

    sema_queue_.En_Q(taskID);
    if (sched_ptr_ != nullptr) {
        sched_ptr_->set_state(taskID, BLOCKED);
    }
    dump(1);

    // The attached lab yields only once here, so a blocked task may stay current
    // until its remaining quantum is exhausted.
    if (sched_ptr_ != nullptr) {
        sched_ptr_->yield();
    }
    dump(1);
}

void semaphore::up() {
    if (sched_ptr_ == nullptr) {
        std::cout << "Invalid semaphore state: missing scheduler link." << std::endl;
        return;
    }

    std::cout << "TaskID : " << sched_ptr_->get_task_id()
              << ", LuckyID : " << lucky_task_ << std::endl;

    if (sched_ptr_->get_task_id() == lucky_task_) {
        if (sema_queue_.IsEmpty()) {
            ++sema_value_;
            if (sema_value_ > 1) {
                sema_value_ = 1;
            }
            lucky_task_ = -1;
            dump(1);
        } else {
            const int task_id = sema_queue_.De_Q();
            sched_ptr_->set_state(task_id, READY);
            std::cout << "Unblock : " << task_id
                      << " and release from the queue" << std::endl;
            lucky_task_ = task_id;
            std::cout << "Luck Task = " << lucky_task_ << std::endl;
            dump(1);
            sched_ptr_->yield();
            dump(1);
        }
    } else {
        std::cout << "Invalid Semaphore UP().  TaskID : " << sched_ptr_->get_task_id()
                  << " Does not own the resource" << std::endl;
        dump(1);
    }
}

void semaphore::dump(const int level) const {
    std::cout << "--------SEMAPHORE DUMP--------" << std::endl;
    switch (level) {
        case 0:
            std::cout << "Sema_Value: " << sema_value_ << std::endl;
            std::cout << "Sema_Name : " << resource_name_ << std::endl;
            std::cout << "Obtained by Task-ID: " << lucky_task_ << std::endl;
            break;
        case 1:
            std::cout << "Sema_Value         : " << sema_value_ << std::endl;
            std::cout << "Sema_Name          : " << resource_name_ << std::endl;
            std::cout << "Obtained by Task-ID: " << lucky_task_ << std::endl;
            std::cout << "Sema_Queue         :" << std::endl;
            sema_queue_.Print_Q();
            break;
        default:
            std::cout << "ERROR in SEMAPHORE DUMP level" << std::endl;
            break;
    }
    std::cout << "------------------------------\n" << std::endl;
}
