#include "lab10_dashboard.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
constexpr int kHeaderHeight = 5;
constexpr int kQueueHeight = 5;
constexpr int kMinPaneHeight = 6;
constexpr int kMinPaneWidth = 24;
constexpr int kFinishPauseMs = 1400;
}

TaskDashboard::TaskDashboard(int taskCount) : taskCount_(taskCount) {
    if (taskCount_ <= 0) {
        throw std::invalid_argument("task dashboard requires at least one task");
    }

    interactive_ = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    if (interactive_) {
        initializeCurses();
        buildLayout();
        logSystem("ncurses dashboard online.");
    } else {
        std::printf("[dashboard] no TTY detected, using transcript mode.\n");
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

void TaskDashboard::finish(const std::string& message) {
    logSystem(message);

    if (interactive_) {
        std::lock_guard<std::mutex> guard(renderMutex_);
        napms(kFinishPauseMs);
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

    systemPane_.title = "System";
    systemPane_.frame = newwin(kHeaderHeight, totalCols, 0, 0);
    systemPane_.body = derwin(systemPane_.frame, kHeaderHeight - 2, totalCols - 2, 1, 1);

    queuePane_.title = "Semaphore Queue";
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
        pane.title = "Task " + std::to_string(taskId);
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
    box(pane.frame, 0, 0);
    mvwprintw(pane.frame, 0, 2, " %s ", pane.title.c_str());
    wrefresh(pane.frame);
}

int TaskDashboard::clampTaskId(int taskId) const {
    return std::clamp(taskId, 0, taskCount_ - 1);
}
