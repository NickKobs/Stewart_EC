#include <cstdlib>
#include <pthread.h>
#include <cstdio>
#include <string>
#include <string_view>
#include <unistd.h>

#include "lab10_dashboard.h"

namespace {
constexpr useconds_t kDefaultDelayMicros = 500000;
pthread_mutex_t g_resource1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_resource2 = PTHREAD_MUTEX_INITIALIZER;

struct Options {
    DashboardMode dashboardMode = DashboardMode::Auto;
    bool holdOnExit = true;
    useconds_t pauseMicros = kDefaultDelayMicros;
};

struct WorkerContext {
    int id = 0;
    const char* workerName = nullptr;
    const char* firstResourceName = nullptr;
    const char* secondResourceName = nullptr;
    pthread_mutex_t* firstResource = nullptr;
    pthread_mutex_t* secondResource = nullptr;
    TaskDashboard* dashboard = nullptr;
    useconds_t pauseMicros = kDefaultDelayMicros;
};

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB22_UI");
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

bool determineHoldOnExit(int argc, char* argv[]) {
    const char* envHold = std::getenv("LAB22_HOLD");
    if (envHold != nullptr) {
        const std::string_view hold(envHold);
        if (hold == "0" || hold == "false" || hold == "FALSE" || hold == "no") {
            return false;
        }
        if (hold == "1" || hold == "true" || hold == "TRUE" || hold == "yes") {
            return true;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--no-hold") {
            return false;
        }
        if (arg == "--hold") {
            return true;
        }
    }

    return true;
}

useconds_t determinePauseMicros(int argc, char* argv[]) {
    const char* envDelay = std::getenv("LAB22_SLEEP_MS");
    if (envDelay != nullptr) {
        const long sleepMs = std::strtol(envDelay, nullptr, 10);
        if (sleepMs >= 0) {
            return static_cast<useconds_t>(sleepMs * 1000);
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--sleep-ms" && i + 1 < argc) {
            const long sleepMs = std::strtol(argv[i + 1], nullptr, 10);
            if (sleepMs >= 0) {
                return static_cast<useconds_t>(sleepMs * 1000);
            }
        }
    }

    return kDefaultDelayMicros;
}

void logSharedState(TaskDashboard& dashboard, const std::string& message) {
    dashboard.logQueue(message);
}

void* worker(void* arg) {
    auto* context = static_cast<WorkerContext*>(arg);
    auto& dashboard = *context->dashboard;

    dashboard.logTask(context->id, "Online.");
    dashboard.logTask(
        context->id,
        std::string("Requesting ") + context->firstResourceName + "."
    );
    pthread_mutex_lock(context->firstResource);

    dashboard.logTask(
        context->id,
        std::string("Acquired ") + context->firstResourceName + "."
    );
    logSharedState(
        dashboard,
        std::string(context->workerName) + " holds " + context->firstResourceName + "."
    );

    dashboard.logTask(
        context->id,
        "Sleeping after the first lock so the other worker can grab its first resource."
    );
    usleep(context->pauseMicros);

    dashboard.logTask(
        context->id,
        std::string("Requesting ") + context->secondResourceName + "."
    );
    logSharedState(
        dashboard,
        std::string(context->workerName) + " is waiting for " + context->secondResourceName +
            " while still holding " + context->firstResourceName + "."
    );

    pthread_mutex_lock(context->secondResource);

    dashboard.logTask(
        context->id,
        std::string("Unexpectedly acquired ") + context->secondResourceName + "."
    );
    dashboard.logTask(context->id, "The deadlock did not happen on this run.");
    pthread_mutex_unlock(context->secondResource);
    pthread_mutex_unlock(context->firstResource);
    return nullptr;
}

Options parseOptions(int argc, char* argv[]) {
    Options options;
    options.dashboardMode = determineDashboardMode(argc, argv);
    options.holdOnExit = determineHoldOnExit(argc, argv);
    options.pauseMicros = determinePauseMicros(argc, argv);
    return options;
}
}

int main(int argc, char* argv[]) {
    const Options options = parseOptions(argc, argv);

    TaskDashboard::DashboardLabels labels;
    labels.systemTitle = "Lab 22 Deadlock";
    labels.sharedTitle = "Resource State";
    labels.taskTitles = {"Worker 1", "Worker 2"};

    TaskDashboard dashboard(2, options.dashboardMode, labels);
    dashboard.setHoldOnExit(options.holdOnExit);

    dashboard.logSystem("Binary deadlock demonstration started.");
    dashboard.logSystem("Worker 1 locks Resource 1 then Resource 2.");
    dashboard.logSystem("Worker 2 locks Resource 2 then Resource 1.");
    dashboard.logSystem(
        "Each worker sleeps after the first lock to force the deadlock window."
    );

    pthread_t workerThreads[2];
    WorkerContext contexts[2] = {
        {
            0,
            "Worker 1",
            "Resource 1",
            "Resource 2",
            &g_resource1,
            &g_resource2,
            &dashboard,
            options.pauseMicros,
        },
        {
            1,
            "Worker 2",
            "Resource 2",
            "Resource 1",
            &g_resource2,
            &g_resource1,
            &dashboard,
            options.pauseMicros,
        },
    };

    for (int i = 0; i < 2; ++i) {
        if (pthread_create(&workerThreads[i], nullptr, worker, &contexts[i]) != 0) {
            std::fprintf(stderr, "Failed to create worker %d\n", i + 1);
            return 1;
        }
    }

    dashboard.logSystem("Main thread now waits on both workers with pthread_join().");
    dashboard.logSystem("A successful deadlock will leave the program blocked here.");

    pthread_join(workerThreads[0], nullptr);
    pthread_join(workerThreads[1], nullptr);

    dashboard.finish("Both workers joined. The deadlock did not occur on this run.");
    return 0;
}
