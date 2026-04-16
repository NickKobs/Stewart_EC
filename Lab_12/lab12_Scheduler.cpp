#include "lab12_Scheduler.h"

#include "lab10_dashboard.h"

#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
constexpr useconds_t kInteractivePauseUs = 80000;

void pause_if_interactive(TaskDashboard& dashboard) {
    if (dashboard.isInteractive()) {
        usleep(kInteractivePauseUs);
    }
}
}

Scheduler::Scheduler(TaskDashboard& dashboard)
    : dashboard_(dashboard) {}

bool Scheduler::create_task() {
    const int slot = find_free_slot();
    dashboard_.logSystem("create_task() invoked.");

    if (slot < 0) {
        dashboard_.logSystem(
            "Create_task() FAILED: Available tasks exceeded. MAX_TASKS = " + std::to_string(MAX_TASKS)
        );
        return false;
    }

    task_table_[slot] = {slot, TaskState::READY, simulated_time_ms_};
    dashboard_.logSystem("Creating task #" + std::to_string(slot));
    dashboard_.logTask(slot, "Created and placed in READY state.");
    return true;
}

bool Scheduler::start() {
    if (current_task_ >= 0) {
        dashboard_.logSystem("start() ignored because the scheduler is already running.");
        return false;
    }

    const int task_id = find_first_ready();
    if (task_id < 0) {
        dashboard_.logSystem("start() FAILED: no READY task is available.");
        return false;
    }

    schedule_task(task_id, "Scheduling started.");
    return true;
}

bool Scheduler::yield() {
    if (current_task_ < 0) {
        dashboard_.logSystem("yield() ignored because there is no RUNNING task.");
        return false;
    }

    const int task_id = current_task_;
    const int elapsed = elapsed_time(task_id);
    dashboard_.logTask(task_id, "Attempting to yield.");
    dashboard_.logSystem(
        "Task " + std::to_string(task_id) + " yield check: elapsed_time=" + std::to_string(elapsed) +
        " current_quantum=" + std::to_string(current_quantum_)
    );

    if (elapsed < current_quantum_) {
        dashboard_.logSystem("Yield ignored because the current quantum has not expired.");
        return false;
    }

    const int next_task = find_next_ready_from(task_id);
    if (next_task < 0) {
        dashboard_.logSystem("No other READY task found. The current task remains RUNNING.");
        task_table_[task_id].start_time = simulated_time_ms_;
        return false;
    }

    task_table_[task_id].state = TaskState::READY;
    schedule_task(next_task, "Quantum expired. Context switch executed.");
    return true;
}

void Scheduler::simulate_cpu_work(const int delta_ms) {
    if (delta_ms < 0) {
        throw std::invalid_argument("simulate_cpu_work() delta must be non-negative");
    }

    simulated_time_ms_ += delta_ms;
    dashboard_.logSystem(
        "Simulated CPU work advanced the scheduler clock by " + std::to_string(delta_ms) +
        " ms. simulated_time=" + std::to_string(simulated_time_ms_)
    );
    pause_if_interactive(dashboard_);
}

void Scheduler::set_quantum(const int quantum_ms) {
    if (quantum_ms <= 0) {
        throw std::invalid_argument("set_quantum() requires a positive quantum");
    }

    current_quantum_ = quantum_ms;
    dashboard_.logSystem("set_quantum(" + std::to_string(quantum_ms) + ") applied.");
}

void Scheduler::dump(const std::string& title) const {
    dashboard_.logQueue("SCHEDULER DUMP: " + title);
    dashboard_.logQueue(
        "simulated_time=" + std::to_string(simulated_time_ms_) +
        " | current_quantum=" + std::to_string(current_quantum_) +
        " | current_task=" + std::to_string(current_task_)
    );

    for (int task_id = 0; task_id < MAX_TASKS; ++task_id) {
        const tcb& entry = task_table_[task_id];
        const int elapsed = elapsed_time(task_id);
        dashboard_.logQueue(
            "task_table[" + std::to_string(task_id) + "] task_id=" + std::to_string(entry.task_id) +
            " state=" + state_to_string(entry.state) +
            " start_time=" + std::to_string(entry.start_time) +
            " elapsed_time=" + std::to_string(elapsed)
        );
    }

    pause_if_interactive(dashboard_);
}

int Scheduler::current_task() const {
    return current_task_;
}

int Scheduler::current_quantum() const {
    return current_quantum_;
}

int Scheduler::simulated_time() const {
    return simulated_time_ms_;
}

int Scheduler::elapsed_time(const int task_id) const {
    const tcb& entry = task(task_id);
    if (entry.state != TaskState::RUNNING) {
        return 0;
    }
    return simulated_time_ms_ - entry.start_time;
}

const tcb& Scheduler::task(const int task_id) const {
    if (task_id < 0 || task_id >= MAX_TASKS) {
        throw std::out_of_range("task index out of range");
    }

    return task_table_[task_id];
}

void Scheduler::block_task(const int task_id, const std::string& reason) {
    if (task_id < 0 || task_id >= MAX_TASKS) {
        throw std::out_of_range("block_task() task index out of range");
    }

    task_table_[task_id].state = TaskState::BLOCKED;
    dashboard_.logTask(task_id, "Moved to BLOCKED.");
    dashboard_.logSystem(
        "Task " + std::to_string(task_id) + " transitioned to BLOCKED because " + reason + '.'
    );

    if (current_task_ != task_id) {
        return;
    }

    current_task_ = -1;
    const int next_task = find_next_ready_from(task_id);
    if (next_task < 0) {
        dashboard_.logSystem("No READY task is available after the block. CPU becomes idle.");
        return;
    }

    schedule_task(next_task, "Forced context switch after task block.");
}

void Scheduler::unblock_task(const int task_id, const std::string& reason) {
    if (task_id < 0 || task_id >= MAX_TASKS) {
        throw std::out_of_range("unblock_task() task index out of range");
    }

    task_table_[task_id].state = TaskState::READY;
    task_table_[task_id].start_time = simulated_time_ms_;
    dashboard_.logTask(task_id, "Moved to READY.");
    dashboard_.logSystem(
        "Task " + std::to_string(task_id) + " transitioned to READY because " + reason + '.'
    );
}

std::string Scheduler::state_to_string(const TaskState state) {
    switch (state) {
        case TaskState::DEAD:
            return "DEAD";
        case TaskState::READY:
            return "READY";
        case TaskState::RUNNING:
            return "RUNNING";
        case TaskState::BLOCKED:
            return "BLOCKED";
    }

    return "UNKNOWN";
}

int Scheduler::find_free_slot() const {
    for (int task_id = 0; task_id < MAX_TASKS; ++task_id) {
        if (task_table_[task_id].state == TaskState::DEAD) {
            return task_id;
        }
    }

    return -1;
}

int Scheduler::find_first_ready() const {
    for (int task_id = 0; task_id < MAX_TASKS; ++task_id) {
        if (task_table_[task_id].state == TaskState::READY) {
            return task_id;
        }
    }

    return -1;
}

int Scheduler::find_next_ready_from(const int task_id) const {
    for (int offset = 1; offset <= MAX_TASKS; ++offset) {
        const int candidate = (task_id + offset) % MAX_TASKS;
        if (task_table_[candidate].state == TaskState::READY) {
            return candidate;
        }
    }

    return -1;
}

void Scheduler::schedule_task(const int task_id, const std::string& reason) {
    if (task_id < 0 || task_id >= MAX_TASKS) {
        throw std::out_of_range("schedule_task() task index out of range");
    }

    current_task_ = task_id;
    task_table_[task_id].state = TaskState::RUNNING;
    task_table_[task_id].start_time = simulated_time_ms_;
    dashboard_.logSystem(reason);
    dashboard_.logSystem("Current task pointer moved to task " + std::to_string(task_id) + '.');
    dashboard_.logTask(task_id, "Scheduled RUNNING.");
    pause_if_interactive(dashboard_);
}
