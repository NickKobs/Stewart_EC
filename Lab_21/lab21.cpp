#include <pthread.h>

#include <array>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "lab10_dashboard.h"

namespace {
constexpr int kThreadCount = 3;
constexpr int kDefaultReceiverReads = 6;
constexpr int kDefaultReceiverDelaySeconds = 5;

struct Message {
    int source = -1;
    int destination = -1;
    std::string data;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

struct Mailbox {
    std::vector<Message> queue;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t notEmpty = PTHREAD_COND_INITIALIZER;
};

struct RunOptions {
    DashboardMode dashboardMode = DashboardMode::Auto;
    bool holdOnExit = true;
    bool showHelp = false;
    bool blockDemo = false;
    int receiverReads = kDefaultReceiverReads;
    int receiverDelaySeconds = kDefaultReceiverDelaySeconds;
};

struct Runtime {
    std::array<Mailbox, kThreadCount> mailboxes {};
    TaskDashboard* dashboard = nullptr;
    RunOptions options;
};

struct ThreadContext {
    Runtime* runtime = nullptr;
    int taskId = -1;
};

void checkPthread(const int status, const std::string& operation) {
    if (status != 0) {
        throw std::runtime_error(operation + " failed: " + std::string(std::strerror(status)));
    }
}

void destroyMailboxPrimitives(Runtime& runtime) {
    for (auto& mailbox : runtime.mailboxes) {
        pthread_cond_destroy(&mailbox.notEmpty);
        pthread_mutex_destroy(&mailbox.mutex);
    }
}

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB21_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "text") {
            return DashboardMode::Text;
        }
        if (mode == "ncurses") {
            return DashboardMode::Ncurses;
        }
        if (mode == "auto") {
            return DashboardMode::Auto;
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
        if (arg == "--auto") {
            return DashboardMode::Auto;
        }
    }

    return DashboardMode::Auto;
}

RunOptions parseArgs(int argc, char* argv[]) {
    RunOptions options;
    options.dashboardMode = determineDashboardMode(argc, argv);

    const char* envHold = std::getenv("LAB21_HOLD");
    if (envHold != nullptr) {
        const std::string_view hold(envHold);
        if (hold == "0" || hold == "false" || hold == "FALSE" || hold == "no") {
            options.holdOnExit = false;
        } else if (hold == "1" || hold == "true" || hold == "TRUE" || hold == "yes") {
            options.holdOnExit = true;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--text" || arg == "--ncurses" || arg == "--auto") {
            continue;
        }

        if (arg == "--hold") {
            options.holdOnExit = true;
            continue;
        }

        if (arg == "--no-hold") {
            options.holdOnExit = false;
            continue;
        }

        if (arg == "--block-demo") {
            options.blockDemo = true;
            continue;
        }

        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
            continue;
        }

        const std::string prefixReads = "--receiver-reads=";
        if (arg.substr(0, prefixReads.size()) == prefixReads) {
            options.receiverReads = std::stoi(std::string(arg.substr(prefixReads.size())));
            continue;
        }

        const std::string prefixDelay = "--receiver-delay=";
        if (arg.substr(0, prefixDelay.size()) == prefixDelay) {
            options.receiverDelaySeconds = std::stoi(std::string(arg.substr(prefixDelay.size())));
            continue;
        }

        throw std::invalid_argument("unknown option: " + std::string(arg));
    }

    if (options.receiverReads < 0) {
        throw std::invalid_argument("receiver read count must be non-negative");
    }

    if (options.receiverDelaySeconds < 0) {
        throw std::invalid_argument("receiver delay must be non-negative");
    }

    return options;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName
              << " [--text|--ncurses|--auto] [--hold|--no-hold]"
              << " [--receiver-reads=N] [--receiver-delay=N] [--block-demo]\n";
    std::cout << "Default behavior follows the lab and applies the safe fix: thread 2 attempts 6 reads\n";
    std::cout << "but checks mailbox 2 before the final read so the program does not block forever.\n";
    std::cout << "Use --block-demo to reproduce the original blocking behavior from the PDF.\n";
}

std::string formatTimestamp(const std::chrono::system_clock::time_point timestamp) {
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()) % std::chrono::seconds(1);
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(timestamp);
    std::tm localTime {};
#if defined(_WIN32)
    localtime_s(&localTime, &timeValue);
#else
    localtime_r(&timeValue, &localTime);
#endif

    std::ostringstream buffer;
    buffer << std::put_time(&localTime, "%H:%M:%S") << '.'
           << std::setw(3) << std::setfill('0') << millis.count();
    return buffer.str();
}

std::string formatMessage(const Message& message) {
    std::ostringstream line;
    line << message.source << "->" << message.destination
         << " @ " << formatTimestamp(message.timestamp)
         << " | " << message.data;
    return line.str();
}

std::vector<std::string> senderMessages(const int taskId) {
    if (taskId == 0) {
        return {
            "Thread 0 message 1/3",
            "Thread 0 message 2/3",
            "Thread 0 message 3/3",
        };
    }

    return {
        "Thread 1 message 1/2",
        "Thread 1 message 2/2",
    };
}

void dump_message_queues(Runtime& runtime, const std::string& reason) {
    runtime.dashboard->logQueue(reason);

    for (int mailboxId = 0; mailboxId < kThreadCount; ++mailboxId) {
        Mailbox& mailbox = runtime.mailboxes[static_cast<std::size_t>(mailboxId)];
        pthread_mutex_lock(&mailbox.mutex);

        std::ostringstream line;
        line << "mailbox[" << mailboxId << "] size=" << mailbox.queue.size() << " | ";
        if (mailbox.queue.empty()) {
            line << "[empty]";
        } else {
            line << '[';
            for (std::size_t i = 0; i < mailbox.queue.size(); ++i) {
                line << formatMessage(mailbox.queue[i]);
                if (i + 1 < mailbox.queue.size()) {
                    line << "; ";
                }
            }
            line << ']';
        }

        pthread_mutex_unlock(&mailbox.mutex);
        runtime.dashboard->logQueue(line.str());
    }
}

void enqueue(Runtime& runtime, const Message& message) {
    Mailbox& mailbox = runtime.mailboxes[static_cast<std::size_t>(message.destination)];

    pthread_mutex_lock(&mailbox.mutex);
    mailbox.queue.push_back(message);
    const std::size_t queueSize = mailbox.queue.size();
    pthread_cond_signal(&mailbox.notEmpty);
    pthread_mutex_unlock(&mailbox.mutex);

    runtime.dashboard->logTask(
        message.source,
        "enqueue() -> mailbox " + std::to_string(message.destination) + ": " + formatMessage(message)
    );
    runtime.dashboard->logTask(
        message.destination,
        "Mailbox accepted message from thread " + std::to_string(message.source) + ". Pending=" + std::to_string(queueSize)
    );
    runtime.dashboard->logSystem(
        "thread " + std::to_string(message.source) + " signaled mailbox " + std::to_string(message.destination)
    );
    dump_message_queues(runtime, "Queue snapshot after enqueue()");
}

Message dequeue(Runtime& runtime, const int taskId) {
    Mailbox& mailbox = runtime.mailboxes[static_cast<std::size_t>(taskId)];
    pthread_mutex_lock(&mailbox.mutex);

    while (mailbox.queue.empty()) {
        runtime.dashboard->logTask(taskId, "Mailbox empty. Waiting on the condition variable.");
        pthread_cond_wait(&mailbox.notEmpty, &mailbox.mutex);
    }

    Message message = mailbox.queue.front();
    mailbox.queue.erase(mailbox.queue.begin());
    pthread_mutex_unlock(&mailbox.mutex);

    runtime.dashboard->logTask(taskId, "dequeue() delivered: " + formatMessage(message));
    dump_message_queues(runtime, "Queue snapshot after dequeue()");
    return message;
}

bool tryDequeue(Runtime& runtime, const int taskId, Message& message) {
    Mailbox& mailbox = runtime.mailboxes[static_cast<std::size_t>(taskId)];
    pthread_mutex_lock(&mailbox.mutex);

    if (mailbox.queue.empty()) {
        pthread_mutex_unlock(&mailbox.mutex);
        return false;
    }

    message = mailbox.queue.front();
    mailbox.queue.erase(mailbox.queue.begin());
    pthread_mutex_unlock(&mailbox.mutex);

    runtime.dashboard->logTask(taskId, "dequeue() delivered: " + formatMessage(message));
    dump_message_queues(runtime, "Queue snapshot after dequeue()");
    return true;
}

void* senderThread(void* rawContext) {
    ThreadContext& context = *static_cast<ThreadContext*>(rawContext);
    Runtime& runtime = *context.runtime;

    runtime.dashboard->logTask(context.taskId, "Sender online.");
    const std::vector<std::string> messages = senderMessages(context.taskId);

    for (std::size_t index = 0; index < messages.size(); ++index) {
        Message message;
        message.source = context.taskId;
        message.destination = 2;
        message.data = messages[index];
        message.timestamp = std::chrono::system_clock::now();

        enqueue(runtime, message);
        runtime.dashboard->logTask(
            context.taskId,
            "Sent message " + std::to_string(index + 1) + "/" + std::to_string(messages.size()) + " to thread 2."
        );

        const int sleepMillis = context.taskId == 0 ? 250 : 400;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMillis));
    }

    runtime.dashboard->logTask(context.taskId, "Sender finished.");
    return nullptr;
}

void* receiverThread(void* rawContext) {
    ThreadContext& context = *static_cast<ThreadContext*>(rawContext);
    Runtime& runtime = *context.runtime;

    runtime.dashboard->logTask(
        context.taskId,
        "Receiver sleeping for " + std::to_string(runtime.options.receiverDelaySeconds) + " seconds before reading mailbox 2."
    );
    std::this_thread::sleep_for(std::chrono::seconds(runtime.options.receiverDelaySeconds));

    runtime.dashboard->logTask(
        context.taskId,
        "Receiver will attempt " + std::to_string(runtime.options.receiverReads) +
            (runtime.options.blockDemo ? " blocking reads." : " reads with a mailbox-empty check.")
    );

    for (int attempt = 0; attempt < runtime.options.receiverReads; ++attempt) {
        if (runtime.options.blockDemo) {
            runtime.dashboard->logTask(
                context.taskId,
                "Attempt " + std::to_string(attempt + 1) + "/" + std::to_string(runtime.options.receiverReads) +
                    ": blocking dequeue()"
            );
            (void)dequeue(runtime, context.taskId);
            continue;
        }

        Message message;
        if (tryDequeue(runtime, context.taskId, message)) {
            runtime.dashboard->logTask(
                context.taskId,
                "Attempt " + std::to_string(attempt + 1) + "/" + std::to_string(runtime.options.receiverReads) +
                    ": processed message from thread " + std::to_string(message.source) + '.'
            );
        } else {
            runtime.dashboard->logTask(
                context.taskId,
                "Attempt " + std::to_string(attempt + 1) + "/" + std::to_string(runtime.options.receiverReads) +
                    ": mailbox 2 is empty, so dequeue() is skipped to avoid the lab's deadlock."
            );
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    runtime.dashboard->logTask(context.taskId, "Receiver finished.");
    return nullptr;
}
}

int main(int argc, char* argv[]) {
    try {
        const RunOptions options = parseArgs(argc, argv);
        if (options.showHelp) {
            printUsage(argv[0]);
            return 0;
        }

        TaskDashboard::DashboardLabels labels;
        labels.systemTitle = "Lab 21 Message Passing";
        labels.sharedTitle = "Mailbox Snapshots";
        labels.taskTitles = {"Thread 0 Sender", "Thread 1 Sender", "Thread 2 Receiver"};

        TaskDashboard dashboard(kThreadCount, options.dashboardMode, labels);
        dashboard.setHoldOnExit(options.holdOnExit);

        Runtime runtime;
        runtime.dashboard = &dashboard;
        runtime.options = options;

        dashboard.logSystem("Lab 21 message-passing simulation online.");
        dashboard.logSystem("Three threads, three mailboxes, one mutex and one condition variable per mailbox.");
        dashboard.logSystem("Thread 0 sends 3 messages to thread 2. Thread 1 sends 2 messages to thread 2.");
        if (options.blockDemo) {
            dashboard.logSystem("block-demo enabled: thread 2 will reproduce the PDF's blocking behavior on the sixth read.");
        } else {
            dashboard.logSystem("Safe mode enabled: thread 2 still attempts 6 reads, but it checks mailbox 2 before dequeue().");
        }
        dump_message_queues(runtime, "Initial queue state");

        std::array<ThreadContext, kThreadCount> contexts {};
        std::array<pthread_t, kThreadCount> threads {};

        for (int taskId = 0; taskId < kThreadCount; ++taskId) {
            contexts[static_cast<std::size_t>(taskId)] = ThreadContext{&runtime, taskId};
        }

        checkPthread(pthread_create(&threads[0], nullptr, senderThread, &contexts[0]), "pthread_create(thread 0)");
        checkPthread(pthread_create(&threads[1], nullptr, senderThread, &contexts[1]), "pthread_create(thread 1)");
        checkPthread(pthread_create(&threads[2], nullptr, receiverThread, &contexts[2]), "pthread_create(thread 2)");

        for (int taskId = 0; taskId < kThreadCount; ++taskId) {
            checkPthread(pthread_join(threads[static_cast<std::size_t>(taskId)], nullptr), "pthread_join(thread " + std::to_string(taskId) + ")");
        }

        dashboard.logSystem("All threads joined.");
        dump_message_queues(runtime, "Final queue state");
        dashboard.finish("Lab 21 complete.");
        destroyMailboxPrimitives(runtime);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "lab21 error: " << error.what() << '\n';
        return 1;
    }
}
