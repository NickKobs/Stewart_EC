#include <pthread.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "lab10_dashboard.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 3
#endif

#ifndef PRODUCE_COUNT
#define PRODUCE_COUNT 10
#endif

#ifndef PRODUCER_SLEEP_SEC
#define PRODUCER_SLEEP_SEC 0L
#endif

#ifndef PRODUCER_SLEEP_NSEC
#define PRODUCER_SLEEP_NSEC 100000000L
#endif

#ifndef CONSUMER_SLEEP_SEC
#define CONSUMER_SLEEP_SEC 0L
#endif

#ifndef CONSUMER_SLEEP_NSEC
#define CONSUMER_SLEEP_NSEC 850000000L
#endif

namespace {
constexpr int kProducerPane = 0;
constexpr int kConsumerPane = 1;
constexpr long kNanosecondsPerSecond = 1000000000L;

int buffer[BUFFER_SIZE] = {};
int in = 0;
int out = 0;
int count = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

TaskDashboard* g_dashboard = nullptr;

TaskDashboard& dashboard() {
    if (g_dashboard == nullptr) {
        throw std::logic_error("lab11 dashboard is not initialized");
    }

    return *g_dashboard;
}

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    if (const char* envMode = std::getenv("LAB11_UI")) {
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
    if (const char* envHold = std::getenv("LAB11_HOLD")) {
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

std::string formatDuration(long sec, long nsec) {
    std::ostringstream duration;
    duration.setf(std::ios::fixed);
    duration.precision(2);
    duration << static_cast<double>(sec * kNanosecondsPerSecond + nsec) / kNanosecondsPerSecond << 's';
    return duration.str();
}

int paneForTask(std::string_view taskName) {
    return taskName == "Producer" ? kProducerPane : kConsumerPane;
}

std::string bufferItemsLocked() {
    std::ostringstream items;
    items << '[';

    int tempOut = out;
    for (int i = 0; i < count; ++i) {
        items << buffer[tempOut];
        if (i + 1 < count) {
            items << ", ";
        }
        tempOut = (tempOut + 1) % BUFFER_SIZE;
    }

    items << ']';
    return items.str();
}

void logBufferState(const std::string& prefix) {
    std::ostringstream line;
    line << prefix
         << " | count=" << count << "/" << BUFFER_SIZE
         << " in=" << in
         << " out=" << out
         << " | items=" << bufferItemsLocked();
    dashboard().logQueue(line.str());
}

void simulate_IO_work(const std::string& taskName, long sec, long nsec) {
    const int pane = paneForTask(taskName);
    dashboard().logTask(pane, "Simulating I/O for " + formatDuration(sec, nsec) + ".");

    struct timespec req {};
    struct timespec rem {};
    req.tv_sec = sec;
    req.tv_nsec = nsec;

    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            dashboard().logTask(pane, "Sleep interrupted; resuming remaining interval.");
            req = rem;
        } else {
            dashboard().logSystem(taskName + " nanosleep failed: " + std::string(std::strerror(errno)));
            break;
        }
    }

    dashboard().logTask(pane, "I/O interval finished.");
}

void* producer(void*) {
    const std::string taskName = "Producer";
    dashboard().logTask(kProducerPane, "Online and ready to produce.");

    for (int item = 0; item < PRODUCE_COUNT; ++item) {
        pthread_mutex_lock(&buffer_mutex);

        while (count == BUFFER_SIZE) {
            dashboard().logTask(kProducerPane, "Buffer full. Waiting on not_full.");
            logBufferState("Producer blocked");
            pthread_cond_wait(&not_full, &buffer_mutex);
            dashboard().logTask(kProducerPane, "Wake-up received; recheck buffer.");
        }

        buffer[in] = item;
        dashboard().logTask(
            kProducerPane,
            "Produced item " + std::to_string(item) + " into slot " + std::to_string(in) + '.'
        );
        in = (in + 1) % BUFFER_SIZE;
        ++count;
        logBufferState("Producer inserted item " + std::to_string(item));

        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&buffer_mutex);

        dashboard().sendTaskMessage(kProducerPane, kConsumerPane, "not_empty signaled.");
        simulate_IO_work(taskName, PRODUCER_SLEEP_SEC, PRODUCER_SLEEP_NSEC);
    }

    dashboard().logTask(kProducerPane, "All items produced.");
    return nullptr;
}

void* consumer(void*) {
    const std::string taskName = "Consumer";
    dashboard().logTask(kConsumerPane, "Online and ready to consume.");

    for (int iteration = 0; iteration < PRODUCE_COUNT; ++iteration) {
        pthread_mutex_lock(&buffer_mutex);

        while (count == 0) {
            dashboard().logTask(kConsumerPane, "Buffer empty. Waiting on not_empty.");
            logBufferState("Consumer blocked");
            pthread_cond_wait(&not_empty, &buffer_mutex);
            dashboard().logTask(kConsumerPane, "Wake-up received; recheck buffer.");
        }

        const int item = buffer[out];
        dashboard().logTask(
            kConsumerPane,
            "Consumed item " + std::to_string(item) + " from slot " + std::to_string(out) + '.'
        );
        out = (out + 1) % BUFFER_SIZE;
        --count;
        logBufferState("Consumer removed item " + std::to_string(item));

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&buffer_mutex);

        dashboard().sendTaskMessage(kConsumerPane, kProducerPane, "not_full signaled.");
        simulate_IO_work(taskName, CONSUMER_SLEEP_SEC, CONSUMER_SLEEP_NSEC);
    }

    dashboard().logTask(kConsumerPane, "All items consumed.");
    return nullptr;
}
}

int main(int argc, char* argv[]) {
    try {
        TaskDashboard::DashboardLabels labels;
        labels.systemTitle = "Lab 11 Producer/Consumer";
        labels.systemSubtitle =
            "Bounded-buffer producer/consumer using one mutex, two condition variables, and nanosleep().";
        labels.sharedTitle = "Buffer State";
        labels.taskTitles = {"Producer", "Consumer"};

        TaskDashboard activeDashboard(2, determineDashboardMode(argc, argv), labels);
        activeDashboard.setHoldOnExit(determineHoldOnExit(argc, argv));
        g_dashboard = &activeDashboard;

        dashboard().logSystem("Launching the bounded-buffer producer/consumer model.");
        {
            std::ostringstream settings;
            settings << "BUFFER_SIZE=" << BUFFER_SIZE
                     << ", PRODUCE_COUNT=" << PRODUCE_COUNT
                     << ", producer_sleep=" << formatDuration(PRODUCER_SLEEP_SEC, PRODUCER_SLEEP_NSEC)
                     << ", consumer_sleep=" << formatDuration(CONSUMER_SLEEP_SEC, CONSUMER_SLEEP_NSEC);
            dashboard().logSystem(settings.str());
        }
        dashboard().logSystem("Condition-variable waits are guarded with while loops for spurious wakeup safety.");
        logBufferState("Initial buffer state");

        pthread_t producerThread {};
        pthread_t consumerThread {};

        int createResult = pthread_create(&producerThread, nullptr, producer, nullptr);
        if (createResult != 0) {
            throw std::runtime_error("pthread_create producer failed: " + std::string(std::strerror(createResult)));
        }

        createResult = pthread_create(&consumerThread, nullptr, consumer, nullptr);
        if (createResult != 0) {
            throw std::runtime_error("pthread_create consumer failed: " + std::string(std::strerror(createResult)));
        }

        dashboard().logSystem("Parent waits for producer and consumer to finish.");

        int joinResult = pthread_join(producerThread, nullptr);
        if (joinResult != 0) {
            throw std::runtime_error("pthread_join producer failed: " + std::string(std::strerror(joinResult)));
        }

        joinResult = pthread_join(consumerThread, nullptr);
        if (joinResult != 0) {
            throw std::runtime_error("pthread_join consumer failed: " + std::string(std::strerror(joinResult)));
        }

        int destroyResult = pthread_mutex_destroy(&buffer_mutex);
        if (destroyResult != 0) {
            throw std::runtime_error("pthread_mutex_destroy failed: " + std::string(std::strerror(destroyResult)));
        }

        destroyResult = pthread_cond_destroy(&not_full);
        if (destroyResult != 0) {
            throw std::runtime_error("pthread_cond_destroy not_full failed: " + std::string(std::strerror(destroyResult)));
        }

        destroyResult = pthread_cond_destroy(&not_empty);
        if (destroyResult != 0) {
            throw std::runtime_error("pthread_cond_destroy not_empty failed: " + std::string(std::strerror(destroyResult)));
        }

        dashboard().finish("Lab 11 run complete.");
        g_dashboard = nullptr;
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "lab11 error: %s\n", error.what());
        return 1;
    }
}
