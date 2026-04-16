#include "lab12_scheduler.h"

#include "lab12_dashboard.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

scheduler::scheduler(Lab12Dashboard* dashboard)
    : current_task_(-1),
      current_quantum_(300),
      logical_clock_(0),
      next_available_task_id_(0),
      dashboard_(dashboard) {
    publish_state();
}

scheduler::~scheduler() {
    emit_system("Exiting Scheduler.....");
}

void scheduler::attach_dashboard(Lab12Dashboard* dashboard) {
    dashboard_ = dashboard;
    publish_state();
}

Lab12Dashboard* scheduler::get_dashboard() const {
    return dashboard_;
}

void scheduler::set_quantum(const std::clock_t quantum) {
    current_quantum_ = quantum;
    publish_state();
}

std::clock_t scheduler::get_quantum() const {
    return current_quantum_;
}

void scheduler::advance_time(const std::clock_t delta) {
    if (delta <= 0) {
        return;
    }

    logical_clock_ += delta;
    publish_state();
}

void scheduler::set_state(const int the_taskid, const std::string& the_state) {
    if (the_taskid < 0 || the_taskid >= MAX_TASKS) {
        return;
    }

    task_table_[the_taskid].state = the_state;
    publish_state();
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
        emit_system("Creating task # " + std::to_string(next_available_task_id_));
        tcb& task = task_table_[next_available_task_id_];
        task.task_id = next_available_task_id_;
        task.state = READY;
        task.start_time = 0;
        task.has_started = false;
        task.next = nullptr;
        emit_task(task.task_id, "Created and placed in READY state.");

        ++next_available_task_id_;
        publish_state();
        return next_available_task_id_ - 1;
    }

    emit_system("Create_task() FAILED: Available tasks exceeded.  MAX_TASKS = " + std::to_string(MAX_TASKS));
    return -1;
}

void scheduler::start() {
    if (next_available_task_id_ == 0) {
        emit_system("Scheduler cannot start: no tasks exist.");
        return;
    }

    emit_system("Scheduling started.");

    task_table_[0].start_time = logical_clock_;
    task_table_[0].has_started = true;
    task_table_[0].state = RUNNING;
    current_task_ = 0;
    set_quantum(1000 / MAX_TASKS);
    emit_task(0, "Started running.");
    publish_state();

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void scheduler::yield() {
    if (current_task_ < 0 || current_task_ >= MAX_TASKS) {
        emit_system("No active task available to yield.");
        return;
    }

    const int previous_task = current_task_;
    int counter = 0;
    emit_task(previous_task, "Attempting to yield.");

    const std::clock_t elapsed_time = elapsed_since(task_table_[current_task_]);
    emit_system(
        "Task " + std::to_string(previous_task) +
        " yield check: elapsed=" + std::to_string(static_cast<long long>(elapsed_time)) +
        " quantum=" + std::to_string(static_cast<long long>(current_quantum_))
    );

    if (elapsed_time >= current_quantum_) {
        emit_task(previous_task, "Quantum expired. Looking for the next READY task.");

        if (task_table_[current_task_].state == RUNNING) {
            task_table_[current_task_].state = READY;
        }

        current_task_ = (current_task_ + 1) % MAX_TASKS;
        while (task_table_[current_task_].state != READY && counter < MAX_TASKS - 1) {
            current_task_ = (current_task_ + 1) % MAX_TASKS;
            ++counter;
        }

        if (counter < MAX_TASKS - 1 && task_table_[current_task_].state == READY) {
            task_table_[current_task_].start_time = logical_clock_;
            task_table_[current_task_].has_started = true;
            task_table_[current_task_].state = RUNNING;
            emit_system(
                "Switching from task " + std::to_string(previous_task) +
                " to task " + std::to_string(current_task_) + "."
            );
            emit_task(current_task_, "Scheduled RUNNING.");
        } else {
            emit_system("POSSIBLE DEAD LOCK");
        }
    } else {
        emit_task(current_task_, "Yield ignored because quantum remains.");
    }

    publish_state();
}

void scheduler::dump() const {
    publish_state();
    if (dashboard_ != nullptr && dashboard_->isInteractive()) {
        return;
    }

    std::cout << "------------------------ PROCESS TABLE ------------------------" << std::endl;
    std::cout << "Quantum = " << current_quantum_ << std::endl;
    std::cout << "Task-ID\tElapsed Time\tState" << std::endl;

    for (int i = 0; i < MAX_TASKS; ++i) {
        const std::clock_t elapsed_time = elapsed_since(task_table_[i]);
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

std::clock_t scheduler::elapsed_since(const tcb& task) const {
    if (!task.has_started) {
        return 0;
    }

    return logical_clock_ - task.start_time;
}

std::vector<std::string> scheduler::build_task_rows() const {
    std::vector<std::string> rows;
    rows.reserve(MAX_TASKS);

    for (int i = 0; i < MAX_TASKS; ++i) {
        std::ostringstream row;
        row << "Task " << task_table_[i].task_id
            << " | " << task_table_[i].state
            << " | elapsed=" << static_cast<long long>(elapsed_since(task_table_[i]));

        if (i == current_task_) {
            row << " | CURRENT";
        }

        rows.push_back(row.str());
    }

    return rows;
}

void scheduler::publish_state() const {
    if (dashboard_ == nullptr) {
        return;
    }

    dashboard_->setSchedulerState(current_quantum_, current_task_, build_task_rows());
}

void scheduler::emit_system(const std::string& message) const {
    if (dashboard_ != nullptr) {
        dashboard_->logSystem(message);
    } else {
        std::cout << message << std::endl;
    }
}

void scheduler::emit_task(const int taskId, const std::string& message) const {
    if (dashboard_ != nullptr) {
        dashboard_->logTask(taskId, message);
    } else {
        std::cout << "[task " << taskId << "] " << message << std::endl;
    }
}
