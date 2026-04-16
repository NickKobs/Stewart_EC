#ifndef EXTRA_CREDIT_LAB12_SCHEDULER_H
#define EXTRA_CREDIT_LAB12_SCHEDULER_H

#include <array>
#include <string>

class TaskDashboard;

enum class TaskState {
    DEAD,
    READY,
    RUNNING,
    BLOCKED,
};

struct tcb {
    int task_id = -1;
    TaskState state = TaskState::DEAD;
    int start_time = 0;
};

class Scheduler {
public:
    static constexpr int MAX_TASKS = 3;
    static constexpr int DEFAULT_QUANTUM = 300;

    explicit Scheduler(TaskDashboard& dashboard);

    bool create_task();
    bool start();
    bool yield();
    void simulate_cpu_work(int delta_ms);
    void set_quantum(int quantum_ms);
    void dump(const std::string& title) const;

    int current_task() const;
    int current_quantum() const;
    int simulated_time() const;
    int elapsed_time(int task_id) const;
    const tcb& task(int task_id) const;

    void block_task(int task_id, const std::string& reason);
    void unblock_task(int task_id, const std::string& reason);

    static std::string state_to_string(TaskState state);

private:
    int find_free_slot() const;
    int find_first_ready() const;
    int find_next_ready_from(int task_id) const;
    void schedule_task(int task_id, const std::string& reason);

    std::array<tcb, MAX_TASKS> task_table_ {};
    int current_task_ = -1;
    int current_quantum_ = DEFAULT_QUANTUM;
    int simulated_time_ms_ = 0;
    TaskDashboard& dashboard_;
};

#endif
