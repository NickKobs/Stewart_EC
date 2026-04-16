#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

#include "lab10_dashboard.h"
#include "lab12_Scheduler.h"
#include "lab12_Semaphore.h"

namespace {
constexpr int kCpuWorkMs = 720;
constexpr int kRoundRobinQuantumMs = 1000 / Scheduler::MAX_TASKS;
constexpr int kExtendedQuantumMs = 2000;

enum class Scenario {
    Full,
    Creation,
    State,
    Semaphore,
    Quantum2000,
};

struct RunOptions {
    DashboardMode mode = DashboardMode::Auto;
    bool hold_on_exit = true;
    bool hold_override = false;
    Scenario scenario = Scenario::Full;
};

Scenario parse_scenario(const std::string_view name) {
    if (name == "full") {
        return Scenario::Full;
    }
    if (name == "creation") {
        return Scenario::Creation;
    }
    if (name == "state") {
        return Scenario::State;
    }
    if (name == "semaphore") {
        return Scenario::Semaphore;
    }
    if (name == "quantum2000") {
        return Scenario::Quantum2000;
    }

    throw std::invalid_argument("unknown --scenario value: " + std::string(name));
}

RunOptions parse_options(const int argc, char* argv[]) {
    RunOptions options;

    if (const char* env_mode = std::getenv("LAB12_UI")) {
        const std::string_view mode(env_mode);
        if (mode == "text") {
            options.mode = DashboardMode::Text;
        } else if (mode == "ncurses") {
            options.mode = DashboardMode::Ncurses;
        }
    }

    if (const char* env_hold = std::getenv("LAB12_HOLD")) {
        const std::string_view hold(env_hold);
        options.hold_override = true;
        options.hold_on_exit = !(hold == "0" || hold == "false" || hold == "FALSE" || hold == "no");
    }

    for (int index = 1; index < argc; ++index) {
        const std::string_view arg(argv[index]);
        if (arg == "--text") {
            options.mode = DashboardMode::Text;
        } else if (arg == "--ncurses") {
            options.mode = DashboardMode::Ncurses;
        } else if (arg == "--hold") {
            options.hold_override = true;
            options.hold_on_exit = true;
        } else if (arg == "--no-hold") {
            options.hold_override = true;
            options.hold_on_exit = false;
        } else if (arg == "--scenario") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--scenario requires a value");
            }
            options.scenario = parse_scenario(argv[++index]);
        } else {
            throw std::invalid_argument("unknown argument: " + std::string(arg));
        }
    }

    return options;
}

void configure_dashboard(TaskDashboard& dashboard, const RunOptions& options) {
    if (options.hold_override) {
        dashboard.setHoldOnExit(options.hold_on_exit);
        return;
    }

    dashboard.setHoldOnExit(dashboard.isInteractive());
}

void create_three_tasks(Scheduler& scheduler) {
    scheduler.create_task();
    scheduler.create_task();
    scheduler.create_task();
}

void run_creation_trace(TaskDashboard& dashboard) {
    dashboard.logSystem("TASK CREATION DUMP: attempting to create 4 tasks while MAX_TASKS = 3.");

    Scheduler scheduler(dashboard);
    Semaphore resource("resource1", 1, dashboard);
    resource.attach_scheduler(&scheduler);

    scheduler.dump("Initial task table");
    for (int attempt = 1; attempt <= 4; ++attempt) {
        scheduler.create_task();
        scheduler.dump("After create_task() attempt #" + std::to_string(attempt));
    }

    resource.dump("Semaphore state after creation attempts");
}

void run_state_trace(TaskDashboard& dashboard) {
    dashboard.logSystem("STATE TRANSITION TRACE: create tasks, start the scheduler, and perform three yields.");

    Scheduler scheduler(dashboard);
    Semaphore resource("resource1", 1, dashboard);
    resource.attach_scheduler(&scheduler);

    create_three_tasks(scheduler);
    scheduler.set_quantum(kRoundRobinQuantumMs);
    scheduler.dump("After set_quantum(1000 / MAX_TASKS)");
    scheduler.start();
    scheduler.dump("After start()");

    for (int pass = 1; pass <= 3; ++pass) {
        scheduler.simulate_cpu_work(kCpuWorkMs);
        scheduler.dump("Before yield() pass #" + std::to_string(pass));
        scheduler.yield();
        scheduler.dump("After yield() pass #" + std::to_string(pass));
    }

    resource.dump("Semaphore state at the end of the state transition trace");
}

void run_semaphore_trace(TaskDashboard& dashboard) {
    dashboard.logSystem("SEMAPHORE QUEUE TRACE: block task 1 and task 2 behind task 0, then restore them to READY.");

    Scheduler scheduler(dashboard);
    Semaphore resource("resource1", 1, dashboard);
    resource.attach_scheduler(&scheduler);

    create_three_tasks(scheduler);
    scheduler.set_quantum(kRoundRobinQuantumMs);
    scheduler.start();
    scheduler.dump("After start()");
    resource.dump("Initial semaphore state");

    resource.down(0);
    scheduler.dump("After task 0 down()");
    resource.dump("After task 0 acquires the semaphore");

    scheduler.simulate_cpu_work(kCpuWorkMs);
    scheduler.yield();
    scheduler.dump("After switching from task 0 to task 1");

    resource.down(1);
    scheduler.dump("After task 1 blocks on down()");
    resource.dump("After task 1 enters the semaphore queue");

    resource.down(2);
    scheduler.dump("After task 2 blocks on down()");
    resource.dump("After task 2 enters the semaphore queue");

    resource.up(0);
    scheduler.dump("After task 0 up() restores task 1 to READY");
    resource.dump("After task 0 releases the semaphore");

    scheduler.simulate_cpu_work(kCpuWorkMs);
    scheduler.yield();
    scheduler.dump("After switching from task 0 to task 1");

    resource.up(1);
    scheduler.dump("After task 1 up() restores task 2 to READY");
    resource.dump("After task 1 releases the semaphore");
}

void run_quantum_trace(TaskDashboard& dashboard) {
    dashboard.logSystem("QUANTUM MODIFICATION TRACE: override the 333 ms quantum and hardcode it to 2000 ms.");

    Scheduler scheduler(dashboard);
    Semaphore resource("resource1", 1, dashboard);
    resource.attach_scheduler(&scheduler);

    create_three_tasks(scheduler);
    scheduler.set_quantum(kRoundRobinQuantumMs);
    scheduler.dump("Baseline dump after set_quantum(1000 / MAX_TASKS)");
    scheduler.set_quantum(kExtendedQuantumMs);
    scheduler.start();
    scheduler.dump("After hardcoding current_quantum to 2000 ms");

    for (int pass = 1; pass <= 3; ++pass) {
        scheduler.simulate_cpu_work(kCpuWorkMs);
        scheduler.dump("Before yield() with 2000 ms quantum, pass #" + std::to_string(pass));
        scheduler.yield();
        scheduler.dump("After yield() with 2000 ms quantum, pass #" + std::to_string(pass));
    }

    resource.dump("Semaphore state during the quantum modification trace");
}

void run_scenario(TaskDashboard& dashboard, const Scenario scenario) {
    switch (scenario) {
        case Scenario::Full:
            run_creation_trace(dashboard);
            run_state_trace(dashboard);
            run_semaphore_trace(dashboard);
            run_quantum_trace(dashboard);
            break;
        case Scenario::Creation:
            run_creation_trace(dashboard);
            break;
        case Scenario::State:
            run_state_trace(dashboard);
            break;
        case Scenario::Semaphore:
            run_semaphore_trace(dashboard);
            break;
        case Scenario::Quantum2000:
            run_quantum_trace(dashboard);
            break;
    }
}
}

int main(int argc, char* argv[]) {
    try {
        const RunOptions options = parse_options(argc, argv);

        TaskDashboard::DashboardLabels labels;
        labels.systemTitle = "Lab 12 Scheduler";
        labels.systemSubtitle =
            "Custom cooperative scheduling with state dumps, semaphore queues, and per-task traces.";
        labels.logTitle = "System";
        labels.sharedTitle = "State Snapshot";
        labels.consoleTitle = "Console";
        labels.taskTitles = {"Task 0", "Task 1", "Task 2"};

        TaskDashboard dashboard(Scheduler::MAX_TASKS, options.mode, labels);
        configure_dashboard(dashboard, options);
        dashboard.logSystem("Lab 12 dashboard online.");
        run_scenario(dashboard, options.scenario);
        dashboard.finish("Lab 12 run complete.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "lab12 error: %s\n", error.what());
        return 1;
    }
}
