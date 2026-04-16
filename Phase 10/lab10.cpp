#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <string>
#include <string_view>
#include <unistd.h>

#include "lab10_dashboard.h"
#include "lab10_semaphore.h"

#ifndef THREAD_COUNT
#define THREAD_COUNT 3
#endif

#ifndef SEMAPHORE_PERMITS
#define SEMAPHORE_PERMITS 1
#endif

#ifndef CRITICAL_SECTION_ROUNDS
#define CRITICAL_SECTION_ROUNDS 4
#endif

struct WorkerContext {
    int id = 0;
    int taskCount = 0;
    Semaphore* semaphore = nullptr;
    TaskDashboard* dashboard = nullptr;
};

DashboardMode determineDashboardMode(int argc, char* argv[]);
void* worker(void* arg);

int main(int argc, char* argv[]) {
    TaskDashboard dashboard(THREAD_COUNT, determineDashboardMode(argc, argv));
    Semaphore semaphore(SEMAPHORE_PERMITS, dashboard);
    pthread_t threads[THREAD_COUNT];
    WorkerContext contexts[THREAD_COUNT];

    dashboard.logSystem("Launching phase 10 semaphore demo.");
    dashboard.logSystem("Each task has its own window. Shared semaphore activity is shown above.");
    for (long i = 0; i < THREAD_COUNT; ++i) {
        contexts[i].id = static_cast<int>(i);
        contexts[i].taskCount = THREAD_COUNT;
        contexts[i].semaphore = &semaphore;
        contexts[i].dashboard = &dashboard;

        if (pthread_create(&threads[i], nullptr, worker, &contexts[i]) != 0) {
            std::fprintf(stderr, "Failed to create thread %ld\n", i);
            return 1;
        }
    }

    dashboard.logSystem("Parent waits for all tasks to finish.");
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(threads[i], nullptr);
    }

    dashboard.finish("All tasks completed. Closing dashboard shortly.");
    return 0;
}

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB10_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "ncurses") {
            return DashboardMode::Ncurses;
        }
        if (mode == "text") {
            return DashboardMode::Text;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--ncurses") {
            return DashboardMode::Ncurses;
        }
        if (arg == "--text") {
            return DashboardMode::Text;
        }
    }

    return DashboardMode::Auto;
}

void* worker(void* arg) {
    auto* context = static_cast<WorkerContext*>(arg);
    const int id = context->id;
    auto& dashboard = *context->dashboard;
    auto& semaphore = *context->semaphore;

    dashboard.logTask(id, "Online and ready.");
    dashboard.sendTaskMessage(id, (id + 1) % context->taskCount, "I am ready to coordinate.");
    dashboard.logTask(id, "Requesting entry to the critical section.");

    semaphore.Down(id);
    dashboard.logTask(id, "ENTER critical section.");

    for (int peer = 0; peer < context->taskCount; ++peer) {
        if (peer == id) {
            continue;
        }

        dashboard.sendTaskMessage(id, peer, "I hold the semaphore now.");
    }

    for (int i = 0; i < CRITICAL_SECTION_ROUNDS; ++i) {
        dashboard.logTask(
            id,
            "Critical work step " + std::to_string(i + 1) + "/" + std::to_string(CRITICAL_SECTION_ROUNDS)
        );
        sleep(1);
    }

    dashboard.logTask(id, "EXIT critical section.");
    semaphore.Up(id);
    dashboard.sendTaskMessage(id, (id + 1) % context->taskCount, "I am done with the semaphore.");
    dashboard.logTask(id, "Finished execution.");

    return nullptr;
}
