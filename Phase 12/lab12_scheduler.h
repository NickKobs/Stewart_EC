#ifndef EXTRA_CREDIT_LAB12_SCHEDULER_H
#define EXTRA_CREDIT_LAB12_SCHEDULER_H

#include <array>
#include <ctime>
#include <string>

constexpr const char* READY = "READY";
constexpr const char* RUNNING = "RUNNING";
constexpr const char* BLOCKED = "BLOCKED";
constexpr const char* DEAD = "DEAD";
constexpr int MAX_TASKS = 3;

struct tcb {
    int task_id = -1;
    std::string state = DEAD;
    std::clock_t start_time = 0;
    tcb* next = nullptr;
};

class scheduler {
public:
    scheduler();
    ~scheduler();

    void set_quantum(std::clock_t quantum);
    [[nodiscard]] std::clock_t get_quantum() const;

    void set_state(int the_taskid, const std::string& the_state);
    [[nodiscard]] std::string get_state(int the_taskid) const;
    [[nodiscard]] int get_task_id() const;

    int create_task();
    void start();
    void yield();
    void dump() const;

private:
    [[nodiscard]] static std::clock_t elapsed_since(std::clock_t start_time);

    int current_task_;
    std::clock_t current_quantum_;
    int next_available_task_id_;
    std::array<tcb, MAX_TASKS> task_table_;
};

#endif
