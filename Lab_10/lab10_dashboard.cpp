#include "lab10_dashboard.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <unistd.h>

namespace {
constexpr int kHeaderHeight = 5;
constexpr int kUpperPaneHeight = 6;
constexpr int kMinPaneHeight = 5;
constexpr int kMinPaneWidth = 20;
constexpr int kMinConsoleWidth = 20;

bool needsDefaultTerm() {
    const char* term = std::getenv("TERM");
    if (term == nullptr || term[0] == '\0') {
        return true;
    }

    const std::string_view termValue(term);
    return termValue == "unknown" || termValue == "dumb";
}
}

TaskDashboard::TaskDashboard(int taskCount, DashboardMode mode)
    : TaskDashboard(taskCount, mode, DashboardLabels{}) {}

TaskDashboard::TaskDashboard(int taskCount, DashboardMode mode, DashboardLabels labels)
    : taskCount_(taskCount), labels_(std::move(labels)) {
    if (taskCount_ <= 0) {
        throw std::invalid_argument("task dashboard requires at least one task");
    }

    if (labels_.systemTitle.empty()) {
        labels_.systemTitle = "Lab Dashboard";
    }

    if (labels_.systemSubtitle.empty()) {
        labels_.systemSubtitle = "Modeled after the Lab 4 / Lab 7 ncurses process-window layout.";
    }

    if (labels_.logTitle.empty()) {
        labels_.logTitle = "Log Window";
    }

    if (labels_.sharedTitle.empty()) {
        labels_.sharedTitle = "Shared State";
    }

    if (labels_.consoleTitle.empty()) {
        labels_.consoleTitle = "Console";
    }

    const bool ttyReady = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    interactive_ = (mode == DashboardMode::Ncurses) || (mode == DashboardMode::Auto && ttyReady);
    interactive_ = interactive_ && ttyReady;
    if (interactive_) {
        if (needsDefaultTerm()) {
            setenv("TERM", "xterm-256color", 1);
        }

        try {
            initializeCurses();
            buildLayout();
            logSystem("ncurses dashboard online.");
        } catch (const std::exception& error) {
            if (cursesReady_) {
                endwin();
                cursesReady_ = false;
            }

            interactive_ = false;
            std::printf("[dashboard] %s Falling back to transcript mode.\n", error.what());
        }
    } else {
        if (mode == DashboardMode::Ncurses) {
            std::printf("[dashboard] ncurses requested but no compatible TTY was detected. ");
            std::printf("Using transcript mode instead.\n");
        } else if (mode == DashboardMode::Auto) {
            std::printf("[dashboard] no compatible TTY detected. ");
            std::printf("Using transcript mode instead.\n");
        } else {
            std::printf("[dashboard] transcript mode active. ");
            std::printf("Run with --ncurses in a real terminal for windowed output.\n");
        }
    }
}

TaskDashboard::~TaskDashboard() {
    if (cursesReady_) {
        std::lock_guard<std::mutex> guard(renderMutex_);
        for (auto& pane : taskPanes_) {
            destroyPane(pane);
        }
        destroyPane(consolePane_);
        destroyPane(queuePane_);
        destroyPane(systemPane_);
        destroyPane(headerPane_);
        endwin();
        cursesReady_ = false;
    }
}

void TaskDashboard::logSystem(const std::string& message) {
    if (!interactive_) {
        std::printf("[system] %s\n", message.c_str());
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    appendLine(systemPane_, message);
}

void TaskDashboard::logQueue(const std::string& message) {
    if (!interactive_) {
        std::printf("[queue] %s\n", message.c_str());
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    appendLine(queuePane_, message);
}

void TaskDashboard::logConsole(const std::string& message) {
    if (!interactive_) {
        std::printf("[console] %s\n", message.c_str());
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    appendLine(consolePane_, message);
}

void TaskDashboard::logTask(int taskId, const std::string& message) {
    const int index = clampTaskId(taskId);

    if (!interactive_) {
        std::printf("[task %d] %s\n", index, message.c_str());
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    appendLine(taskPanes_[static_cast<std::size_t>(index)], message);
}

void TaskDashboard::sendTaskMessage(int fromTaskId, int toTaskId, const std::string& message) {
    const int fromIndex = clampTaskId(fromTaskId);
    const int toIndex = clampTaskId(toTaskId);

    std::ostringstream outbound;
    outbound << "To task " << toIndex << ": " << message;
    logTask(fromIndex, outbound.str());

    std::ostringstream inbound;
    inbound << "From task " << fromIndex << ": " << message;
    logTask(toIndex, inbound.str());

    std::ostringstream systemEvent;
    systemEvent << "task " << fromIndex << " -> task " << toIndex << " | " << message;
    logSystem(systemEvent.str());
}

bool TaskDashboard::isInteractive() const {
    return interactive_;
}

std::string TaskDashboard::promptInput(const std::string& prompt, int maxLength) {
    if (!interactive_) {
        std::printf("%s", prompt.c_str());
        std::fflush(stdout);

        std::string line;
        if (!std::getline(std::cin, line)) {
            return {};
        }

        return line;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);

    const int safeMaxLength = std::max(1, maxLength);
    int bodyRows = 0;
    int bodyCols = 0;
    getmaxyx(consolePane_.body, bodyRows, bodyCols);

    renderTitle(consolePane_);
    werase(consolePane_.body);
    mvwprintw(consolePane_.body, 0, 0, "%.*s", std::max(1, bodyCols - 1), prompt.c_str());

    const int inputRow = bodyRows > 1 ? 1 : 0;
    const int inputCol = bodyRows > 1 ? 0 : std::min(static_cast<int>(prompt.size()), std::max(0, bodyCols - 2));
    wmove(consolePane_.body, inputRow, inputCol);
    wclrtoeol(consolePane_.body);
    wrefresh(consolePane_.body);

    keypad(consolePane_.body, TRUE);
    echo();
    curs_set(1);

    std::vector<char> buffer(static_cast<std::size_t>(safeMaxLength) + 1U, '\0');
    const int status = wgetnstr(consolePane_.body, buffer.data(), safeMaxLength);

    noecho();
    curs_set(0);
    refreshConsole();

    if (status == ERR) {
        return {};
    }

    return std::string(buffer.data());
}

void TaskDashboard::setHoldOnExit(bool holdOnExit) {
    holdOnExit_ = holdOnExit;
}

void TaskDashboard::finish(const std::string& message) {
    logSystem(message);

    if (interactive_ && holdOnExit_) {
        logConsole("Press Enter, q, or Esc to exit.");
        std::lock_guard<std::mutex> guard(renderMutex_);
        int key = 0;
        while (key != '\n' && key != '\r' && key != 'q' && key != 'Q' && key != 27) {
            key = wgetch(consolePane_.body);
        }
    }
}

void TaskDashboard::initializeCurses() {
    if (initscr() == nullptr) {
        throw std::runtime_error("failed to initialize ncurses");
    }

    cursesReady_ = true;
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, FALSE);
    refresh();
}

void TaskDashboard::buildLayout() {
    const int totalRows = LINES;
    const int totalCols = COLS;
    const int bodyTop = kHeaderHeight + kUpperPaneHeight;
    const int bodyHeight = totalRows - bodyTop;
    const int preferredCols = taskCount_ == 1 ? 1 : (taskCount_ <= 4 ? 2 : 3);
    const int taskCols = std::max(1, std::min(preferredCols, std::max(1, totalCols / kMinPaneWidth)));
    const int taskRows = static_cast<int>(std::ceil(static_cast<double>(taskCount_) / taskCols));

    if (bodyHeight < taskRows * kMinPaneHeight) {
        throw std::runtime_error("terminal is too small for the shared Lab 4 / Lab 7 ncurses layout");
    }

    const int consoleWidth = std::max(kMinConsoleWidth, std::min(28, totalCols / 4));
    const int centerWidth = totalCols - consoleWidth;
    const int systemWidth = centerWidth / 2;
    const int sharedWidth = centerWidth - systemWidth;
    if (systemWidth < kMinPaneWidth || sharedWidth < kMinPaneWidth) {
        throw std::runtime_error("terminal is too small for the log/shared/console panes");
    }

    headerPane_.title = labels_.systemTitle;
    headerPane_.frame = newwin(kHeaderHeight, totalCols, 0, 0);
    headerPane_.body = derwin(headerPane_.frame, kHeaderHeight - 2, totalCols - 2, 1, 1);

    systemPane_.title = labels_.logTitle;
    systemPane_.frame = newwin(kUpperPaneHeight, systemWidth, kHeaderHeight, 0);
    systemPane_.body = derwin(systemPane_.frame, kUpperPaneHeight - 2, systemWidth - 2, 1, 1);

    queuePane_.title = labels_.sharedTitle;
    queuePane_.frame = newwin(kUpperPaneHeight, sharedWidth, kHeaderHeight, systemWidth);
    queuePane_.body = derwin(queuePane_.frame, kUpperPaneHeight - 2, sharedWidth - 2, 1, 1);

    consolePane_.title = labels_.consoleTitle;
    consolePane_.frame = newwin(kUpperPaneHeight, consoleWidth, kHeaderHeight, systemWidth + sharedWidth);
    consolePane_.body = derwin(consolePane_.frame, kUpperPaneHeight - 2, consoleWidth - 2, 1, 1);

    renderHeader();
    renderTitle(systemPane_);
    renderTitle(queuePane_);
    renderTitle(consolePane_);
    scrollok(systemPane_.body, TRUE);
    scrollok(queuePane_.body, TRUE);
    scrollok(consolePane_.body, TRUE);
    refreshConsole();

    taskPanes_.resize(static_cast<std::size_t>(taskCount_));
    const int paneHeight = bodyHeight / taskRows;
    const int paneWidth = totalCols / taskCols;

    for (int taskId = 0; taskId < taskCount_; ++taskId) {
        const int row = taskId / taskCols;
        const int col = taskId % taskCols;
        const int paneY = bodyTop + row * paneHeight;
        const int paneX = col * paneWidth;
        const int actualHeight = (row == taskRows - 1) ? totalRows - paneY : paneHeight;
        const int actualWidth = (col == taskCols - 1) ? totalCols - paneX : paneWidth;

        auto& pane = taskPanes_[static_cast<std::size_t>(taskId)];
        if (taskId < static_cast<int>(labels_.taskTitles.size()) && !labels_.taskTitles[taskId].empty()) {
            pane.title = labels_.taskTitles[taskId];
        } else {
            pane.title = "Task " + std::to_string(taskId);
        }
        pane.frame = newwin(actualHeight, actualWidth, paneY, paneX);
        pane.body = derwin(pane.frame, actualHeight - 2, actualWidth - 2, 1, 1);
        scrollok(pane.body, TRUE);
        renderTitle(pane);
    }
}

void TaskDashboard::destroyPane(LogPane& pane) {
    if (pane.body != nullptr) {
        delwin(pane.body);
        pane.body = nullptr;
    }

    if (pane.frame != nullptr) {
        delwin(pane.frame);
        pane.frame = nullptr;
    }
}

void TaskDashboard::appendLine(LogPane& pane, const std::string& message) {
    renderTitle(pane);
    wprintw(pane.body, "%s\n", message.c_str());
    wrefresh(pane.body);
}

void TaskDashboard::renderHeader() {
    werase(headerPane_.frame);
    wborder(headerPane_.frame, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(headerPane_.frame, 0, 2, " %s ", headerPane_.title.c_str());

    werase(headerPane_.body);
    int bodyRows = 0;
    int bodyCols = 0;
    getmaxyx(headerPane_.body, bodyRows, bodyCols);

    mvwprintw(headerPane_.body, 0, 0, "%.*s", std::max(1, bodyCols - 1), labels_.systemSubtitle.c_str());

    if (bodyRows > 1) {
        const std::string modeLine = interactive_
            ? "ncurses active | Lab 7-style serialized window writes | console pane handles prompts and exit"
            : "transcript fallback active | run inside a TTY for the ncurses process-model view";
        mvwprintw(headerPane_.body, 1, 0, "%.*s", std::max(1, bodyCols - 1), modeLine.c_str());
    }

    if (bodyRows > 2) {
        std::ostringstream line;
        line << "task panes=" << taskCount_ << " | top row=" << labels_.logTitle << ", " << labels_.sharedTitle
             << ", " << labels_.consoleTitle;
        mvwprintw(headerPane_.body, 2, 0, "%.*s", std::max(1, bodyCols - 1), line.str().c_str());
    }

    wrefresh(headerPane_.frame);
    wrefresh(headerPane_.body);
}

void TaskDashboard::renderTitle(LogPane& pane) {
    werase(pane.frame);
    wborder(pane.frame, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(pane.frame, 0, 2, " %s ", pane.title.c_str());
    wrefresh(pane.frame);
}

void TaskDashboard::refreshConsole() {
    renderTitle(consolePane_);
    werase(consolePane_.body);

    const std::array<std::string, 4> defaultLines = {
        "Lab 4 / Lab 7 console",
        "prompts render here",
        "close: Enter/q/Esc",
        interactive_ ? "TTY attached" : "transcript mode",
    };

    int bodyRows = 0;
    int bodyCols = 0;
    getmaxyx(consolePane_.body, bodyRows, bodyCols);
    for (int row = 0; row < bodyRows && row < static_cast<int>(defaultLines.size()); ++row) {
        mvwprintw(consolePane_.body, row, 0, "%.*s", std::max(1, bodyCols - 1), defaultLines[row].c_str());
    }

    wrefresh(consolePane_.body);
}

int TaskDashboard::clampTaskId(int taskId) const {
    return std::clamp(taskId, 0, taskCount_ - 1);
}
