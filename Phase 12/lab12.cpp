#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>

#include "lab12_dashboard.h"
#include "lab12_scheduler.h"
#include "lab12_semaphore.h"

Lab12DashboardMode determineDashboardMode(int argc, char* argv[]);
void logSystem(Lab12Dashboard& dashboard, const std::string& message);
void logTask(Lab12Dashboard& dashboard, int taskId, const std::string& message);

void waste_time(const int x, Lab12Dashboard& dashboard, scheduler& swapper, const int taskId) {
    logTask(dashboard, taskId, "Simulating CPU work.");

    volatile unsigned long long accumulator = 0;
    for (unsigned int i = 0; i < 30000U * static_cast<unsigned int>(x); ++i) {
        for (unsigned short j = 1; j > 0; --j) {
            accumulator += j + i;
        }
    }

    swapper.advance_time(240 * x);
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    (void)accumulator;
}

int main(int argc, char* argv[]) {
    Lab12Dashboard dashboard(MAX_TASKS, determineDashboardMode(argc, argv));
    scheduler swapper(&dashboard);
    int t_id = -1;

    logSystem(dashboard, "Step 1 & 2: Create a scheduler and attempt to create 4 tasks.");
    for (int i = 0; i < 4; ++i) {
        t_id = swapper.create_task();
    }
    (void)t_id;

    swapper.dump();

    logSystem(dashboard, "Step 3: Start the scheduler.");
    swapper.start();
    swapper.dump();

    logSystem(dashboard, "Step 4: Try some context switching (3 yields).");
    for (int i = 0; i < 3; ++i) {
        waste_time(3, dashboard, swapper, swapper.get_task_id());
        swapper.yield();
        swapper.dump();
    }

    logSystem(dashboard, "Step 5: Test the scheduler and semaphore together.");
    semaphore resource1_sema(1, "resource1", &swapper);
    resource1_sema.dump(0);

    t_id = swapper.get_task_id();
    logTask(dashboard, t_id, "Trying to obtain semaphore resource1.");
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3, dashboard, swapper, swapper.get_task_id());
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    logTask(dashboard, t_id, "Trying to obtain semaphore resource1.");
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3, dashboard, swapper, swapper.get_task_id());
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    logTask(dashboard, t_id, "Trying to obtain semaphore resource1.");
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3, dashboard, swapper, swapper.get_task_id());
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    logTask(dashboard, t_id, "Trying to release semaphore resource1.");
    swapper.dump();
    resource1_sema.up();
    swapper.dump();
    waste_time(3, dashboard, swapper, swapper.get_task_id());
    swapper.yield();
    swapper.dump();

    dashboard.finish("Lab 12 run complete. Closing dashboard shortly.");
    return 0;
}

Lab12DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB12_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "text") {
            return Lab12DashboardMode::Text;
        }
        if (mode == "ncurses") {
            return Lab12DashboardMode::Ncurses;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--text") {
            return Lab12DashboardMode::Text;
        }
        if (arg == "--ncurses") {
            return Lab12DashboardMode::Ncurses;
        }
    }

    return Lab12DashboardMode::Ncurses;
}

void logSystem(Lab12Dashboard& dashboard, const std::string& message) {
    dashboard.logSystem(message);
}

void logTask(Lab12Dashboard& dashboard, const int taskId, const std::string& message) {
    dashboard.logTask(taskId, message);
}
