#include "lab12_semaphore.h"

#include "lab12_dashboard.h"

#include <iostream>
#include <string>
#include <utility>

semaphore::semaphore(const int starting_value, std::string name, scheduler* theScheduler)
    : resource_name_(std::move(name)),
      sema_value_(starting_value),
      lucky_task_(-1),
      sched_ptr_(theScheduler) {
    publish_state();
}

void semaphore::down(const int taskID) {
    emit_task(taskID, "Requesting semaphore " + resource_name_ + ".");

    if (taskID == lucky_task_) {
        emit_task(taskID, "Already owns the semaphore. Ignoring duplicate request.");
        dump(1);
        return;
    }

    if (sema_value_ >= 1) {
        --sema_value_;
        lucky_task_ = taskID;
        emit_task(taskID, "Obtained semaphore " + resource_name_ + ".");
        emit_system("Task " + std::to_string(taskID) + " obtained " + resource_name_ + ".");
        dump(1);
        return;
    }

    sema_queue_.En_Q(taskID);
    if (sched_ptr_ != nullptr) {
        sched_ptr_->set_state(taskID, BLOCKED);
    }
    emit_task(taskID, "Blocked and queued for " + resource_name_ + ".");
    emit_system("Task " + std::to_string(taskID) + " blocked on " + resource_name_ + ".");
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
        emit_system("Invalid semaphore state: missing scheduler link.");
        return;
    }

    emit_system(
        "Semaphore release check: task=" + std::to_string(sched_ptr_->get_task_id()) +
        " owner=" + std::to_string(lucky_task_)
    );

    if (sched_ptr_->get_task_id() == lucky_task_) {
        if (sema_queue_.IsEmpty()) {
            ++sema_value_;
            if (sema_value_ > 1) {
                sema_value_ = 1;
            }
            emit_task(lucky_task_, "Released semaphore " + resource_name_ + ".");
            lucky_task_ = -1;
            dump(1);
        } else {
            const int task_id = sema_queue_.De_Q();
            sched_ptr_->set_state(task_id, READY);
            emit_system("Unblock task " + std::to_string(task_id) + " from " + resource_name_ + ".");
            lucky_task_ = task_id;
            emit_task(task_id, "Woken from semaphore queue and moved to READY.");
            dump(1);
            sched_ptr_->yield();
            dump(1);
        }
    } else {
        emit_system(
            "Invalid Semaphore UP(). Task " + std::to_string(sched_ptr_->get_task_id()) +
            " does not own " + resource_name_ + "."
        );
        dump(1);
    }
}

void semaphore::dump(const int level) const {
    publish_state();
    if (sched_ptr_ != nullptr) {
        auto* dashboard = sched_ptr_->get_dashboard();
        if (dashboard != nullptr && dashboard->isInteractive()) {
            return;
        }
    }

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

void semaphore::publish_state() const {
    if (sched_ptr_ == nullptr) {
        return;
    }

    Lab12Dashboard* dashboard = sched_ptr_->get_dashboard();
    if (dashboard == nullptr) {
        return;
    }

    dashboard->setSemaphoreState(resource_name_, sema_value_, lucky_task_, sema_queue_.Get_Q_String());
}

void semaphore::emit_system(const std::string& message) const {
    if (sched_ptr_ != nullptr) {
        Lab12Dashboard* dashboard = sched_ptr_->get_dashboard();
        if (dashboard != nullptr) {
            dashboard->logSystem(message);
            return;
        }
    }

    std::cout << message << std::endl;
}

void semaphore::emit_task(const int taskId, const std::string& message) const {
    if (sched_ptr_ != nullptr) {
        Lab12Dashboard* dashboard = sched_ptr_->get_dashboard();
        if (dashboard != nullptr) {
            dashboard->logTask(taskId, message);
            return;
        }
    }

    std::cout << "[task " << taskId << "] " << message << std::endl;
}
