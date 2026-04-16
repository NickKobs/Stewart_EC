#include "lab10_dashboard.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <unistd.h>

namespace {
constexpr int kHeaderHeight = 5;
constexpr int kQueueHeight = 5;
constexpr int kMinPaneHeight = 6;
constexpr int kMinPaneWidth = 24;

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
        labels_.systemTitle = "System";
    }

    if (labels_.sharedTitle.empty()) {
        labels_.sharedTitle = "Shared State";
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
        destroyPane(queuePane_);
        destroyPane(systemPane_);
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

void TaskDashboard::setHoldOnExit(bool holdOnExit) {
    holdOnExit_ = holdOnExit;
}

void TaskDashboard::finish(const std::string& message) {
    logSystem(message);

    if (interactive_) {
        std::lock_guard<std::mutex> guard(renderMutex_);
        if (holdOnExit_) {
            appendLine(systemPane_, "Press Enter, q, or Esc to exit.");
            int key = 0;
            while (key != '\n' && key != '\r' && key != 'q' && key != 'Q' && key != 27) {
                key = wgetch(stdscr);
            }
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
    const int bodyTop = kHeaderHeight + kQueueHeight;
    const int bodyHeight = totalRows - bodyTop;
    const int preferredCols = taskCount_ == 1 ? 1 : (taskCount_ <= 4 ? 2 : 3);
    const int taskCols = std::max(1, std::min(preferredCols, std::max(1, totalCols / kMinPaneWidth)));
    const int taskRows = static_cast<int>(std::ceil(static_cast<double>(taskCount_) / taskCols));

    if (bodyHeight < taskRows * kMinPaneHeight) {
        throw std::runtime_error("terminal is too small for the phase 10 ncurses layout");
    }

    systemPane_.title = labels_.systemTitle;
    systemPane_.frame = newwin(kHeaderHeight, totalCols, 0, 0);
    systemPane_.body = derwin(systemPane_.frame, kHeaderHeight - 2, totalCols - 2, 1, 1);

    queuePane_.title = labels_.sharedTitle;
    queuePane_.frame = newwin(kQueueHeight, totalCols, kHeaderHeight, 0);
    queuePane_.body = derwin(queuePane_.frame, kQueueHeight - 2, totalCols - 2, 1, 1);

    renderTitle(systemPane_);
    renderTitle(queuePane_);
    scrollok(systemPane_.body, TRUE);
    scrollok(queuePane_.body, TRUE);

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

void TaskDashboard::renderTitle(LogPane& pane) {
    werase(pane.frame);
    wborder(pane.frame, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(pane.frame, 0, 2, " %s ", pane.title.c_str());
    wrefresh(pane.frame);
}

int TaskDashboard::clampTaskId(int taskId) const {
    return std::clamp(taskId, 0, taskCount_ - 1);
}
