#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <ncurses.h>
#include <sstream>
#include <string>

namespace {
constexpr long long kLoopIterations = 1000000000LL;
constexpr int kMinimumRows = 24;
constexpr int kMinimumCols = 80;

WINDOW* g_headerWindow = nullptr;
WINDOW* g_wallClockWindow = nullptr;
WINDOW* g_cpuSecondsWindow = nullptr;
WINDOW* g_cpuMicrosecondsWindow = nullptr;

void drawPaneFrame(WINDOW* window, const char* title) {
    werase(window);
    box(window, 0, 0);
    mvwprintw(window, 0, 2, " %s ", title);
}

void printPaneLine(WINDOW* window, const int row, const std::string& text) {
    mvwprintw(window, row, 2, "%s", text.c_str());
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

void renderHeader(const std::string& status, const bool readyForInput) {
    drawPaneFrame(g_headerWindow, "Lab 18 System Time (ncurses)");
    printPaneLine(g_headerWindow, 1, "Wall clock + CPU timing dashboard");
    printPaneLine(g_headerWindow, 2, "time()/localtime(), clock(), std::chrono");
    printPaneLine(g_headerWindow, 3, readyForInput ? status + "  r reruns, q quits." : status);
    wrefresh(g_headerWindow);
}

int displayWallTime(void) {
    drawPaneFrame(g_wallClockWindow, "Wall-Clock Time");

    const std::time_t rawTime = std::time(nullptr);
    if (rawTime == static_cast<std::time_t>(-1)) {
        printPaneLine(g_wallClockWindow, 2, "time() failed.");
        wrefresh(g_wallClockWindow);
        return 1;
    }

    const std::tm* timeInfo = std::localtime(&rawTime);
    if (timeInfo == nullptr) {
        printPaneLine(g_wallClockWindow, 2, "localtime() failed.");
        wrefresh(g_wallClockWindow);
        return 1;
    }

    char formattedTime[64] = {};
    std::strftime(formattedTime, sizeof(formattedTime), "%a %b %d %H:%M:%S %Y", timeInfo);

    printPaneLine(g_wallClockWindow, 2, "Current local time and date:");
    printPaneLine(g_wallClockWindow, 3, formattedTime);
    printPaneLine(
        g_wallClockWindow,
        5,
        "tm_year/tm_mon/tm_mday : " + std::to_string(1900 + timeInfo->tm_year) + "/" +
            std::to_string(1 + timeInfo->tm_mon) + "/" + std::to_string(timeInfo->tm_mday)
    );
    printPaneLine(
        g_wallClockWindow,
        6,
        "tm_hour/tm_min/tm_sec  : " + std::to_string(timeInfo->tm_hour) + ":" +
            std::to_string(timeInfo->tm_min) + ":" + std::to_string(timeInfo->tm_sec)
    );
    wrefresh(g_wallClockWindow);
    return 0;
}

int displayCpuTime_ForMyLoopInSeconds(void) {
    drawPaneFrame(g_cpuSecondsWindow, "CPU Time via clock()");
    printPaneLine(g_cpuSecondsWindow, 2, "clock() timing for 1B-loop:");
    printPaneLine(g_cpuSecondsWindow, 4, "Running workload...");
    wrefresh(g_cpuSecondsWindow);

    const std::clock_t start = std::clock();
    const std::uint64_t checksum = runLoopWorkload();
    const std::clock_t end = std::clock();
    const double cpuTimeUsed = static_cast<double>(end - start) / CLOCKS_PER_SEC;

    drawPaneFrame(g_cpuSecondsWindow, "CPU Time via clock()");
    printPaneLine(g_cpuSecondsWindow, 2, "clock() timing for 1B-loop:");
    printPaneLine(g_cpuSecondsWindow, 4, "Loop time: " + formatFixed(cpuTimeUsed) + " seconds");
    printPaneLine(g_cpuSecondsWindow, 6, "Checksum: " + std::to_string(checksum));
    wrefresh(g_cpuSecondsWindow);
    return 0;
}

int displayCpuTime_ForMyLoopInMicroseconds() {
    drawPaneFrame(g_cpuMicrosecondsWindow, "CPU Time via std::chrono");
    printPaneLine(g_cpuMicrosecondsWindow, 2, "std::chrono timing for 1B-loop:");
    printPaneLine(g_cpuMicrosecondsWindow, 4, "Running workload...");
    wrefresh(g_cpuMicrosecondsWindow);

    const auto start = std::chrono::high_resolution_clock::now();
    const std::uint64_t checksum = runLoopWorkload();
    const auto end = std::chrono::high_resolution_clock::now();

    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double seconds = static_cast<double>(duration.count()) / 1000000.0;

    drawPaneFrame(g_cpuMicrosecondsWindow, "CPU Time via std::chrono");
    printPaneLine(g_cpuMicrosecondsWindow, 2, "std::chrono timing for 1B-loop:");
    printPaneLine(
        g_cpuMicrosecondsWindow,
        4,
        "Execution time: " + std::to_string(duration.count()) + " microseconds"
    );
    printPaneLine(g_cpuMicrosecondsWindow, 5, "Execution time: " + formatFixed(seconds) + " seconds");
    printPaneLine(g_cpuMicrosecondsWindow, 6, "Checksum: " + std::to_string(checksum));
    printPaneLine(g_cpuMicrosecondsWindow, 7, "volatile is used in the loop so the workload remains observable.");
    wrefresh(g_cpuMicrosecondsWindow);
    return 0;
}

void runMeasurements() {
    renderHeader("Sampling wall-clock time...", false);
    const int wallClockResult = displayWallTime();

    renderHeader("Measuring CPU time with clock()...", false);
    const int secondsResult = displayCpuTime_ForMyLoopInSeconds();

    renderHeader("Measuring CPU time with std::chrono...", false);
    const int microsecondsResult = displayCpuTime_ForMyLoopInMicroseconds();

    if (wallClockResult == 0 && secondsResult == 0 && microsecondsResult == 0) {
        renderHeader("Measurements complete.", true);
        return;
    }

    renderHeader("One or more measurements failed.", true);
}
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);

    if (rows < kMinimumRows || cols < kMinimumCols) {
        clear();
        mvprintw(1, 2, "Terminal too small for Lab 18 ncurses dashboard.");
        mvprintw(2, 2, "Minimum size: %d rows x %d columns", kMinimumRows, kMinimumCols);
        mvprintw(3, 2, "Current size: %d rows x %d columns", rows, cols);
        mvprintw(5, 2, "Press any key to exit.");
        refresh();
        getch();
        endwin();
        return 1;
    }

    constexpr int headerHeight = 5;
    constexpr int topPaneHeight = 11;
    const int topPaneWidth = cols / 2;
    const int bottomPaneHeight = rows - headerHeight - topPaneHeight;

    g_headerWindow = newwin(headerHeight, cols, 0, 0);
    g_wallClockWindow = newwin(topPaneHeight, topPaneWidth, headerHeight, 0);
    g_cpuSecondsWindow = newwin(topPaneHeight, cols - topPaneWidth, headerHeight, topPaneWidth);
    g_cpuMicrosecondsWindow = newwin(bottomPaneHeight, cols, headerHeight + topPaneHeight, 0);

    runMeasurements();

    while (true) {
        const int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            break;
        }

        if (ch == 'r' || ch == 'R') {
            runMeasurements();
        }
    }

    delwin(g_cpuMicrosecondsWindow);
    delwin(g_cpuSecondsWindow);
    delwin(g_wallClockWindow);
    delwin(g_headerWindow);
    endwin();
    return 0;
}
