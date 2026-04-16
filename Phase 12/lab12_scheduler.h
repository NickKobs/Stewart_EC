#ifndef EXTRA_CREDIT_LAB12_SCHEDULER_H
#define EXTRA_CREDIT_LAB12_SCHEDULER_H

#include <array>
#include <ctime>
#include <string>
#include <vector>

class Lab12Dashboard;

constexpr const char* READY = "READY";
constexpr const char* RUNNING = "RUNNING";
constexpr const char* BLOCKED = "BLOCKED";
constexpr const char* DEAD = "DEAD";
constexpr int MAX_TASKS = 3;

struct tcb {
    int task_id = -1;
    std::string state = DEAD;
    std::clock_t start_time = 0;
    bool has_started = false;
    tcb* next = nullptr;
};

class scheduler {
public:
    explicit scheduler(Lab12Dashboard* dashboard = nullptr);
    ~scheduler();

    void attach_dashboard(Lab12Dashboard* dashboard);
    [[nodiscard]] Lab12Dashboard* get_dashboard() const;

    void set_quantum(std::clock_t quantum);
    [[nodiscard]] std::clock_t get_quantum() const;
    void advance_time(std::clock_t delta);

    void set_state(int the_taskid, const std::string& the_state);
    [[nodiscard]] std::string get_state(int the_taskid) const;
    [[nodiscard]] int get_task_id() const;

    int create_task();
    void start();
    void yield();
    void dump() const;

private:
    [[nodiscard]] std::clock_t elapsed_since(const tcb& task) const;
    [[nodiscard]] std::vector<std::string> build_task_rows() const;
    void publish_state() const;
    void emit_system(const std::string& message) const;
    void emit_task(int taskId, const std::string& message) const;

    int current_task_;
    std::clock_t current_quantum_;
    std::clock_t logical_clock_;
    int next_available_task_id_;
    std::array<tcb, MAX_TASKS> task_table_;
    Lab12Dashboard* dashboard_;
};

#endif
