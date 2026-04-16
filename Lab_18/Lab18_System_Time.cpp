#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "lab10_dashboard.h"

namespace {
constexpr long long kLoopIterations = 1000000000LL;
constexpr int kWallClockPane = 0;
constexpr int kClockPane = 1;
constexpr int kChronoPane = 2;

enum class MeasurementSelection {
    All,
    WallOnly,
    CpuOnly,
};

struct WallClockSample {
    bool ok = false;
    std::time_t epochSeconds = 0;
    std::tm localTime {};
    std::string formattedTime;
};

struct CpuClockSample {
    bool ok = false;
    double seconds = 0.0;
    std::uint64_t checksum = 0;
};

struct CpuChronoSample {
    bool ok = false;
    long long microseconds = 0;
    double seconds = 0.0;
    std::uint64_t checksum = 0;
};

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    if (const char* envMode = std::getenv("LAB18_UI")) {
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

    return DashboardMode::Auto;
}

bool determineHoldOnExit(int argc, char* argv[]) {
    if (const char* envHold = std::getenv("LAB18_HOLD")) {
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

MeasurementSelection determineMeasurementSelection(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--wall-only") {
            return MeasurementSelection::WallOnly;
        }
        if (arg == "--cpu-only") {
            return MeasurementSelection::CpuOnly;
        }
    }

    return MeasurementSelection::All;
}

std::string formatFixed(const double value) {
    std::ostringstream buffer;
    buffer << std::fixed << std::setprecision(6) << value;
    return buffer.str();
}

// Keep the loop result observable so the compiler cannot discard the work.
std::uint64_t runLoopWorkload() {
    volatile std::uint64_t sum = 0;
    for (long long i = 0; i < kLoopIterations; ++i) {
        sum += static_cast<std::uint64_t>(i);
    }

    return sum;
}

WallClockSample displayWallTime(TaskDashboard& dashboard, const int runNumber) {
    dashboard.logTask(kWallClockPane, "Run " + std::to_string(runNumber) + ": sampling wall-clock time.");

    WallClockSample sample;
    const std::time_t rawTime = std::time(nullptr);
    if (rawTime == static_cast<std::time_t>(-1)) {
        dashboard.logTask(kWallClockPane, "time() failed.");
        return sample;
    }

    const std::tm* timeInfo = std::localtime(&rawTime);
    if (timeInfo == nullptr) {
        dashboard.logTask(kWallClockPane, "localtime() failed.");
        return sample;
    }

    sample.ok = true;
    sample.epochSeconds = rawTime;
    sample.localTime = *timeInfo;
    char formattedTime[64] = {};
    std::strftime(formattedTime, sizeof(formattedTime), "%a %b %d %H:%M:%S %Y", timeInfo);
    sample.formattedTime = formattedTime;

    dashboard.logTask(kWallClockPane, "Unix Epoch seconds: " + std::to_string(static_cast<long long>(sample.epochSeconds)));
    dashboard.logTask(kWallClockPane, "Current local time and date: " + sample.formattedTime);
    dashboard.logTask(kWallClockPane, "tm_year = " + std::to_string(1900 + sample.localTime.tm_year));
    dashboard.logTask(kWallClockPane, "tm_mon  = " + std::to_string(1 + sample.localTime.tm_mon));
    dashboard.logTask(kWallClockPane, "tm_mday = " + std::to_string(sample.localTime.tm_mday));
    dashboard.logTask(kWallClockPane, "tm_hour = " + std::to_string(sample.localTime.tm_hour));
    dashboard.logTask(kWallClockPane, "tm_min  = " + std::to_string(sample.localTime.tm_min));
    dashboard.logTask(kWallClockPane, "tm_sec  = " + std::to_string(sample.localTime.tm_sec));
    return sample;
}

CpuClockSample displayCpuTime_ForMyLoopInSeconds(TaskDashboard& dashboard, const int runNumber) {
    dashboard.logTask(kClockPane, "Run " + std::to_string(runNumber) + ": measuring the 1B loop via clock().");

    CpuClockSample sample;
    const std::clock_t start = std::clock();
    sample.checksum = runLoopWorkload();
    const std::clock_t end = std::clock();
    sample.seconds = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    sample.ok = true;

    dashboard.logTask(kClockPane, "Testing the execution time of a loop (in Seconds):");
    dashboard.logTask(kClockPane, "Loop iterations: " + std::to_string(kLoopIterations));
    dashboard.logTask(kClockPane, "Execution time: " + formatFixed(sample.seconds) + " seconds");
    dashboard.logTask(kClockPane, "Checksum: " + std::to_string(sample.checksum));
    return sample;
}

CpuChronoSample displayCpuTime_ForMyLoopInMicroseconds(TaskDashboard& dashboard, const int runNumber) {
    dashboard.logTask(kChronoPane, "Run " + std::to_string(runNumber) + ": measuring the 1B loop via std::chrono.");

    CpuChronoSample sample;
    const auto start = std::chrono::high_resolution_clock::now();
    sample.checksum = runLoopWorkload();
    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    sample.microseconds = duration.count();
    sample.seconds = static_cast<double>(duration.count()) / 1000000.0;
    sample.ok = true;

    dashboard.logTask(kChronoPane, "Testing the execution time of a loop (in MicroSeconds):");
    dashboard.logTask(kChronoPane, "Loop iterations: " + std::to_string(kLoopIterations));
    dashboard.logTask(kChronoPane, "Execution time: " + std::to_string(sample.microseconds) + " microseconds");
    dashboard.logTask(kChronoPane, "Execution time: " + formatFixed(sample.seconds) + " seconds");
    dashboard.logTask(kChronoPane, "Checksum: " + std::to_string(sample.checksum));
    dashboard.logTask(kChronoPane, "volatile keeps the workload observable to the compiler.");
    return sample;
}

void logCpuDifferential(TaskDashboard& dashboard, const CpuClockSample& clockSample, const CpuChronoSample& chronoSample) {
    if (!clockSample.ok || !chronoSample.ok) {
        return;
    }

    dashboard.logQueue(
        "CPU differential summary | clock() = " + formatFixed(clockSample.seconds) + " seconds | std::chrono = " +
            std::to_string(chronoSample.microseconds) + " microseconds (" + formatFixed(chronoSample.seconds) + " seconds)"
    );
    dashboard.logQueue(
        "Observed delta (chrono_seconds - clock_seconds) = " +
            formatFixed(chronoSample.seconds - clockSample.seconds) + " seconds"
    );
}

void runMeasurements(TaskDashboard& dashboard, const int runNumber, const MeasurementSelection selection) {
    dashboard.logSystem("Run " + std::to_string(runNumber) + " started.");

    if (selection == MeasurementSelection::WallOnly) {
        dashboard.logQueue("Wall-clock-only trace requested.");
        const WallClockSample wallClockSample = displayWallTime(dashboard, runNumber);

        if (wallClockSample.ok) {
            dashboard.logSystem("Wall-clock trace complete.");
            dashboard.logConsole("Wall-clock trace complete. Use r to rerun or q to quit.");
            return;
        }

        dashboard.logSystem("Wall-clock trace failed.");
        dashboard.logConsole("Wall-clock trace failed. Use r to rerun or q to quit.");
        return;
    }

    if (selection == MeasurementSelection::CpuOnly) {
        dashboard.logQueue("CPU-only differential trace requested.");
        const CpuClockSample clockSample = displayCpuTime_ForMyLoopInSeconds(dashboard, runNumber);
        const CpuChronoSample chronoSample = displayCpuTime_ForMyLoopInMicroseconds(dashboard, runNumber);
        logCpuDifferential(dashboard, clockSample, chronoSample);

        if (clockSample.ok && chronoSample.ok) {
            dashboard.logSystem("CPU differential trace complete.");
            dashboard.logConsole("CPU trace complete. Use r to rerun or q to quit.");
            return;
        }

        dashboard.logSystem("CPU differential trace failed.");
        dashboard.logConsole("CPU trace failed. Use r to rerun or q to quit.");
        return;
    }

    dashboard.logQueue("Run " + std::to_string(runNumber) + " | wall-clock time, clock(), and std::chrono.");

    const WallClockSample wallClockSample = displayWallTime(dashboard, runNumber);
    const CpuClockSample clockSample = displayCpuTime_ForMyLoopInSeconds(dashboard, runNumber);
    const CpuChronoSample chronoSample = displayCpuTime_ForMyLoopInMicroseconds(dashboard, runNumber);
    logCpuDifferential(dashboard, clockSample, chronoSample);

    if (wallClockSample.ok && clockSample.ok && chronoSample.ok) {
        dashboard.logSystem("Run " + std::to_string(runNumber) + " complete.");
        dashboard.logConsole("Run complete. Use r to rerun or q to quit.");
        return;
    }

    dashboard.logSystem("Run " + std::to_string(runNumber) + " completed with one or more timing failures.");
    dashboard.logConsole("One or more timing calls failed. Use r to rerun or q to quit.");
}
}

int main(int argc, char* argv[]) {
    try {
        TaskDashboard::DashboardLabels labels;
        labels.systemTitle = "Lab 18 System Time";
        labels.systemSubtitle = "Wall-clock timing with time()/localtime() plus CPU timing via clock() and std::chrono.";
        labels.sharedTitle = "Differential Notes";
        labels.taskTitles = {"Wall-Clock Time", "CPU Time via clock()", "CPU Time via std::chrono"};

        TaskDashboard dashboard(3, determineDashboardMode(argc, argv), labels);
        const bool holdOnExit = determineHoldOnExit(argc, argv);
        const MeasurementSelection selection = determineMeasurementSelection(argc, argv);
        dashboard.setHoldOnExit(holdOnExit);

        int runNumber = 1;
        runMeasurements(dashboard, runNumber, selection);

        if (dashboard.isInteractive() && holdOnExit) {
            while (true) {
                const std::string command = dashboard.promptInput("Command (r to rerun, q to quit): ", 8);
                if (command.empty()) {
                    continue;
                }

                const char action = static_cast<char>(std::tolower(static_cast<unsigned char>(command.front())));
                if (action == 'q') {
                    break;
                }

                if (action == 'r') {
                    ++runNumber;
                    runMeasurements(dashboard, runNumber, selection);
                    continue;
                }

                dashboard.logConsole("Unknown command: " + command);
            }

            dashboard.setHoldOnExit(false);
        }

        dashboard.finish("Lab 18 session complete.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "lab18 error: %s\n", error.what());
        return 1;
    }
}
