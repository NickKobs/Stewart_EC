#include <array>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "../queue.h"

namespace {
constexpr int kPhilosopherCount = 5;
constexpr int kDefaultMeals = 3;
constexpr int kDefaultThinkMilliseconds = 75;
constexpr int kDefaultEatMilliseconds = 75;

std::mutex g_outputMutex;

struct Options {
    int meals = kDefaultMeals;
    int thinkMilliseconds = kDefaultThinkMilliseconds;
    int eatMilliseconds = kDefaultEatMilliseconds;
    bool showHelp = false;
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [--meals N] [--think-ms N] [--eat-ms N]\n";
    std::cout << "Runs a nurse-controlled Dining Philosophers simulation with 5 philosophers.\n";
}

void logLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_outputMutex);
    std::cout << line << '\n';
}

int parsePositiveInt(const char* rawValue, const std::string& optionName) {
    try {
        const int value = std::stoi(rawValue);
        if (value <= 0) {
            throw std::invalid_argument("value must be positive");
        }

        return value;
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid value for " + optionName + ": " + rawValue);
    }
}

Options parseArguments(int argc, char* argv[]) {
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);

        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            continue;
        }

        if (index + 1 >= argc) {
            throw std::invalid_argument("missing value for option " + std::string(argument));
        }

        if (argument == "--meals") {
            options.meals = parsePositiveInt(argv[++index], "--meals");
            continue;
        }

        if (argument == "--think-ms") {
            options.thinkMilliseconds = parsePositiveInt(argv[++index], "--think-ms");
            continue;
        }

        if (argument == "--eat-ms") {
            options.eatMilliseconds = parsePositiveInt(argv[++index], "--eat-ms");
            continue;
        }

        throw std::invalid_argument("unknown option: " + std::string(argument));
    }

    return options;
}

constexpr int leftFork(const int philosopherId) {
    return philosopherId;
}

constexpr int rightFork(const int philosopherId) {
    return (philosopherId + 1) % kPhilosopherCount;
}

class NurseTeam {
public:
    void requestMeal(const int philosopherId) {
        std::unique_lock<std::mutex> lock(mutex_);

        requestQueue_.En_Q(philosopherId);
        logLocked(
            "Philosopher " + std::to_string(philosopherId) +
            " checks in with the nurse team for forks " +
            std::to_string(leftFork(philosopherId)) + " and " +
            std::to_string(rightFork(philosopherId)) +
            ". Queue: " + requestQueue_.Get_Q_String()
        );

        condition_.wait(lock, [&] {
            return requestQueue_.Front() == philosopherId &&
                   !forkInUse_[leftFork(philosopherId)] &&
                   !forkInUse_[rightFork(philosopherId)];
        });

        requestQueue_.De_Q();
        forkInUse_[leftFork(philosopherId)] = true;
        forkInUse_[rightFork(philosopherId)] = true;

        logLocked(
            "Nurse team grants philosopher " + std::to_string(philosopherId) +
            " forks " + std::to_string(leftFork(philosopherId)) + " and " +
            std::to_string(rightFork(philosopherId)) +
            ". Queue: " + requestQueue_.Get_Q_String()
        );
    }

    void finishMeal(const int philosopherId) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            forkInUse_[leftFork(philosopherId)] = false;
            forkInUse_[rightFork(philosopherId)] = false;
            ++mealsServed_[philosopherId];

            logLocked(
                "Philosopher " + std::to_string(philosopherId) +
                " returns forks " + std::to_string(leftFork(philosopherId)) + " and " +
                std::to_string(rightFork(philosopherId)) +
                " to the nurse team."
            );
        }

        condition_.notify_all();
    }

    std::array<int, kPhilosopherCount> mealsServed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mealsServed_;
    }

private:
    void logLocked(const std::string& line) const {
        std::lock_guard<std::mutex> lock(g_outputMutex);
        std::cout << line << '\n';
    }

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::array<bool, kPhilosopherCount> forkInUse_{};
    std::array<int, kPhilosopherCount> mealsServed_{};
    Queue<int> requestQueue_;
};

void philosopherTask(const int philosopherId, const Options& options, NurseTeam& nurseTeam) {
    for (int mealNumber = 1; mealNumber <= options.meals; ++mealNumber) {
        logLine(
            "Philosopher " + std::to_string(philosopherId) +
            " is THINKING before meal " + std::to_string(mealNumber) + "."
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(options.thinkMilliseconds));

        nurseTeam.requestMeal(philosopherId);

        logLine(
            "    Philosopher " + std::to_string(philosopherId) +
            " is EATING meal " + std::to_string(mealNumber) + "."
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(options.eatMilliseconds));

        nurseTeam.finishMeal(philosopherId);
    }

    logLine("Philosopher " + std::to_string(philosopherId) + " completed all assigned meals.");
}
}

int main(int argc, char* argv[]) {
    try {
        const Options options = parseArguments(argc, argv);
        if (options.showHelp) {
            printUsage(argv[0]);
            return 0;
        }

        logLine("Lab 23 nurse-controlled Dining Philosophers simulation");
        logLine(
            "Configuration: philosophers=" + std::to_string(kPhilosopherCount) +
            ", meals=" + std::to_string(options.meals) +
            ", think-ms=" + std::to_string(options.thinkMilliseconds) +
            ", eat-ms=" + std::to_string(options.eatMilliseconds)
        );

        NurseTeam nurseTeam;
        std::vector<std::thread> philosophers;
        philosophers.reserve(kPhilosopherCount);

        for (int philosopherId = 0; philosopherId < kPhilosopherCount; ++philosopherId) {
            philosophers.emplace_back(philosopherTask, philosopherId, std::cref(options), std::ref(nurseTeam));
        }

        for (std::thread& philosopher : philosophers) {
            philosopher.join();
        }

        const std::array<int, kPhilosopherCount> mealsServed = nurseTeam.mealsServed();
        logLine("Summary:");
        for (int philosopherId = 0; philosopherId < kPhilosopherCount; ++philosopherId) {
            logLine(
                "Philosopher " + std::to_string(philosopherId) +
                " ate " + std::to_string(mealsServed[philosopherId]) + " time(s)."
            );
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "lab23 error: " << error.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }
}
