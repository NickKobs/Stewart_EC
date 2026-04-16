#include "lab12_scheduler.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

scheduler::scheduler()
    : current_task_(-1), current_quantum_(300), next_available_task_id_(0) {}

scheduler::~scheduler() {
    std::cout << "Exiting Scheduler....." << std::endl;
}

void scheduler::set_quantum(const std::clock_t quantum) {
    current_quantum_ = quantum;
}

std::clock_t scheduler::get_quantum() const {
    return current_quantum_;
}

void scheduler::set_state(const int the_taskid, const std::string& the_state) {
    if (the_taskid < 0 || the_taskid >= MAX_TASKS) {
        return;
    }

    task_table_[the_taskid].state = the_state;
}

std::string scheduler::get_state(const int the_taskid) const {
    if (the_taskid < 0 || the_taskid >= MAX_TASKS) {
        return DEAD;
    }

    return task_table_[the_taskid].state;
}

int scheduler::get_task_id() const {
    return current_task_;
}

int scheduler::create_task() {
    if (next_available_task_id_ < MAX_TASKS) {
        std::cout << "Creating task # " << next_available_task_id_ << std::endl;
        tcb& task = task_table_[next_available_task_id_];
        task.task_id = next_available_task_id_;
        task.state = READY;
        task.start_time = 0;
        task.next = nullptr;

        ++next_available_task_id_;
        return next_available_task_id_ - 1;
    }

    std::cout << "Create_task() FAILED: Available tasks exceeded.  MAX_TASKS = "
              << MAX_TASKS << std::endl;
    return -1;
}

void scheduler::start() {
    if (next_available_task_id_ == 0) {
        std::cout << "Scheduler cannot start: no tasks exist." << std::endl;
        return;
    }

    std::cout << "............." << std::endl;
    std::cout << ".....SCHEDULING STARTED" << std::endl;
    std::cout << ".............\n" << std::endl;

    task_table_[0].start_time = std::clock();
    task_table_[0].state = RUNNING;
    current_task_ = 0;
    set_quantum(1000 / MAX_TASKS);

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void scheduler::yield() {
    if (current_task_ < 0 || current_task_ >= MAX_TASKS) {
        std::cout << "No active task available to yield." << std::endl;
        return;
    }

    int counter = 0;
    std::cout << "Current Task # " << current_task_ << " is trying to yield" << std::endl;

    const std::clock_t elapsed_time = elapsed_since(task_table_[current_task_].start_time);
    std::cout << "Task: " << current_task_ << " Elapsed time: " << elapsed_time << std::endl;
    std::cout << "Current Quantum : " << current_quantum_ << std::endl;

    if (elapsed_time >= current_quantum_) {
        std::cout << "Yielding....(Switching from task # " << current_task_
                  << " to next ready task)" << std::endl;

        if (task_table_[current_task_].state == RUNNING) {
            task_table_[current_task_].state = READY;
        }

        current_task_ = (current_task_ + 1) % MAX_TASKS;
        while (task_table_[current_task_].state != READY && counter < MAX_TASKS - 1) {
            current_task_ = (current_task_ + 1) % MAX_TASKS;
            ++counter;
        }

        if (counter < MAX_TASKS - 1 && task_table_[current_task_].state == READY) {
            task_table_[current_task_].start_time = std::clock();
            task_table_[current_task_].state = RUNNING;
            std::cout << "Started Running task # " << current_task_ << std::endl;
        } else {
            std::cout << "POSSIBLE DEAD LOCK" << std::endl;
        }
    } else {
        std::cout << "NO Yield!  (task: " << current_task_
                  << " Still have some quantum left)" << std::endl;
    }
}

void scheduler::dump() const {
    std::cout << "------------------------ PROCESS TABLE ------------------------" << std::endl;
    std::cout << "Quantum = " << current_quantum_ << std::endl;
    std::cout << "Task-ID\tElapsed Time\tState" << std::endl;

    for (int i = 0; i < MAX_TASKS; ++i) {
        const std::clock_t elapsed_time = elapsed_since(task_table_[i].start_time);
        std::cout << std::setw(6) << task_table_[i].task_id << '\t'
                  << std::setw(12) << elapsed_time << '\t'
                  << task_table_[i].state;

        if (i == current_task_) {
            std::cout << "  <-- CURRENT PROCESS";
        }

        std::cout << std::endl;
    }

    std::cout << "--------------------------------------------------------------\n" << std::endl;
}

std::clock_t scheduler::elapsed_since(const std::clock_t start_time) {
    if (start_time == 0) {
        return 0;
    }

    return std::clock() - start_time;
}
