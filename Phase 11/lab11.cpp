#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <pthread.h>
#include <sstream>
#include <string>
#include <string_view>
#include <time.h>

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
constexpr int kProducerTaskId = 0;
constexpr int kConsumerTaskId = 1;
constexpr int kTaskCount = 2;
constexpr long kNanosecondsPerSecond = 1000000000L;

TaskDashboard* g_dashboard = nullptr;

struct ThreadContext {
    int taskId = 0;
    std::string taskName;
};

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB11_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "text") {
            return DashboardMode::Text;
        }
        if (mode == "ncurses") {
            return DashboardMode::Ncurses;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--text") {
            return DashboardMode::Text;
        }
        if (arg == "--ncurses") {
            return DashboardMode::Ncurses;
        }
    }

    return DashboardMode::Ncurses;
}

std::string formatDuration(long sec, long nsec) {
    std::ostringstream builder;
    builder << std::fixed << std::setprecision(2)
            << static_cast<double>(sec * kNanosecondsPerSecond + nsec) / kNanosecondsPerSecond
            << "s";
    return builder.str();
}

std::string bufferSnapshotLocked(const std::string& event, int buffer[], int bufferCount, int in, int out) {
    std::ostringstream builder;
    builder << event << " | count=" << bufferCount << "/" << BUFFER_SIZE << " in=" << in << " out=" << out
            << " | items=[";

    for (int index = 0; index < bufferCount; ++index) {
        if (index > 0) {
            builder << ", ";
        }

        builder << buffer[(out + index) % BUFFER_SIZE];
    }

    builder << "]";
    return builder.str();
}
}

int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;
int buffer_count = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

void* producer(void* arg);
void* consumer(void* arg);
void simulate_IO_work(const ThreadContext& context, long sec, long nsec);

int main(int argc, char* argv[]) {
    TaskDashboard dashboard(
        kTaskCount,
        determineDashboardMode(argc, argv),
        {
            .systemTitle = "Phase 11 Producer/Consumer",
            .sharedTitle = "Buffer State",
            .taskTitles = {"Producer", "Consumer"},
        }
    );
    g_dashboard = &dashboard;

    pthread_t prod;
    pthread_t cons;
    ThreadContext producerContext {kProducerTaskId, "Producer"};
    ThreadContext consumerContext {kConsumerTaskId, "Consumer"};

    dashboard.logSystem("Launching Lab 11 producer/consumer dashboard.");
    dashboard.logSystem("Each task has its own window. The shared pane shows the circular buffer.");
    {
        std::ostringstream builder;
        builder << "BUFFER_SIZE=" << BUFFER_SIZE << ", PRODUCE_COUNT=" << PRODUCE_COUNT
                << ", producer_sleep=" << formatDuration(PRODUCER_SLEEP_SEC, PRODUCER_SLEEP_NSEC)
                << ", consumer_sleep=" << formatDuration(CONSUMER_SLEEP_SEC, CONSUMER_SLEEP_NSEC);
        dashboard.logSystem(builder.str());
    }
    dashboard.logQueue("Initial buffer state | count=0/" + std::to_string(BUFFER_SIZE) + " in=0 out=0 | items=[]");

    int create_result = pthread_create(&prod, nullptr, producer, &producerContext);
    if (create_result != 0) {
        std::fprintf(stderr, "pthread_create producer failed: %s\n", strerror(create_result));
        return EXIT_FAILURE;
    }

    create_result = pthread_create(&cons, nullptr, consumer, &consumerContext);
    if (create_result != 0) {
        std::fprintf(stderr, "pthread_create consumer failed: %s\n", strerror(create_result));
        return EXIT_FAILURE;
    }

    dashboard.logSystem("Parent waits for producer and consumer to finish.");

    int join_result = pthread_join(prod, nullptr);
    if (join_result != 0) {
        std::fprintf(stderr, "pthread_join producer failed: %s\n", strerror(join_result));
        return EXIT_FAILURE;
    }

    join_result = pthread_join(cons, nullptr);
    if (join_result != 0) {
        std::fprintf(stderr, "pthread_join consumer failed: %s\n", strerror(join_result));
        return EXIT_FAILURE;
    }

    dashboard.logSystem("Threads completed. Destroying mutex and condition variables.");

    int destroy_result = pthread_mutex_destroy(&buffer_mutex);
    if (destroy_result != 0) {
        std::fprintf(stderr, "pthread_mutex_destroy failed: %s\n", strerror(destroy_result));
        return EXIT_FAILURE;
    }

    destroy_result = pthread_cond_destroy(&not_empty);
    if (destroy_result != 0) {
        std::fprintf(stderr, "pthread_cond_destroy not_empty failed: %s\n", strerror(destroy_result));
        return EXIT_FAILURE;
    }

    destroy_result = pthread_cond_destroy(&not_full);
    if (destroy_result != 0) {
        std::fprintf(stderr, "pthread_cond_destroy not_full failed: %s\n", strerror(destroy_result));
        return EXIT_FAILURE;
    }

    dashboard.finish("Phase 11 run complete. Closing dashboard shortly.");
    g_dashboard = nullptr;
    return EXIT_SUCCESS;
}

void* producer(void* arg) {
    auto* context = static_cast<ThreadContext*>(arg);
    g_dashboard->logTask(context->taskId, "Online and ready to produce.");

    for (int i = 0; i < PRODUCE_COUNT; ++i) {
        pthread_mutex_lock(&buffer_mutex);

        while (buffer_count == BUFFER_SIZE) {
            g_dashboard->logTask(context->taskId, "Buffer full. Waiting on not_full.");
            g_dashboard->logQueue(bufferSnapshotLocked("Producer blocked", buffer, buffer_count, in, out));
            pthread_cond_wait(&not_full, &buffer_mutex);
            g_dashboard->logTask(context->taskId, "Wake-up received; recheck buffer.");
        }

        const int writeIndex = in;
        buffer[in] = i;
        in = (in + 1) % BUFFER_SIZE;
        ++buffer_count;

        {
            std::ostringstream event;
            event << "Produced item " << i << " into slot " << writeIndex << ".";
            g_dashboard->logTask(context->taskId, event.str());
        }
        g_dashboard->logQueue(bufferSnapshotLocked("Producer inserted item " + std::to_string(i), buffer, buffer_count, in, out));
        g_dashboard->sendTaskMessage(context->taskId, kConsumerTaskId, "not_empty signaled.");

        pthread_mutex_unlock(&buffer_mutex);
        pthread_cond_signal(&not_empty);

        simulate_IO_work(*context, PRODUCER_SLEEP_SEC, PRODUCER_SLEEP_NSEC);
    }

    g_dashboard->logTask(context->taskId, "All items produced.");
    return nullptr;
}

void* consumer(void* arg) {
    auto* context = static_cast<ThreadContext*>(arg);
    g_dashboard->logTask(context->taskId, "Online and ready to consume.");

    for (int i = 0; i < PRODUCE_COUNT; ++i) {
        pthread_mutex_lock(&buffer_mutex);

        while (buffer_count == 0) {
            g_dashboard->logTask(context->taskId, "Buffer empty. Waiting on not_empty.");
            g_dashboard->logQueue(bufferSnapshotLocked("Consumer blocked", buffer, buffer_count, in, out));
            pthread_cond_wait(&not_empty, &buffer_mutex);
            g_dashboard->logTask(context->taskId, "Wake-up received; recheck buffer.");
        }

        const int readIndex = out;
        const int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        --buffer_count;

        {
            std::ostringstream event;
            event << "Consumed item " << item << " from slot " << readIndex << ".";
            g_dashboard->logTask(context->taskId, event.str());
        }
        g_dashboard->logQueue(bufferSnapshotLocked("Consumer removed item " + std::to_string(item), buffer, buffer_count, in, out));
        g_dashboard->sendTaskMessage(context->taskId, kProducerTaskId, "not_full signaled.");

        pthread_mutex_unlock(&buffer_mutex);
        pthread_cond_signal(&not_full);

        simulate_IO_work(*context, CONSUMER_SLEEP_SEC, CONSUMER_SLEEP_NSEC);
    }

    g_dashboard->logTask(context->taskId, "All items consumed.");
    return nullptr;
}

void simulate_IO_work(const ThreadContext& context, long sec, long nsec) {
    struct timespec req {};
    struct timespec rem {};

    req.tv_sec = sec;
    req.tv_nsec = nsec;

    g_dashboard->logTask(context.taskId, "Simulating I/O for " + formatDuration(sec, nsec) + ".");

    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            g_dashboard->logTask(context.taskId, "Sleep interrupted; resuming remaining interval.");
            req = rem;
        } else {
            perror("nanosleep");
            break;
        }
    }

    g_dashboard->logTask(context.taskId, "I/O interval finished.");
}
