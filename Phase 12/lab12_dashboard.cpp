#include "lab12_dashboard.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {
constexpr int kHeaderHeight = 5;
constexpr int kStateHeight = 8;
constexpr int kMinPaneHeight = 6;
constexpr int kMinPaneWidth = 24;
constexpr int kFinishPauseMs = 1400;

bool needsDefaultTerm() {
    const char* term = std::getenv("TERM");
    if (term == nullptr || term[0] == '\0') {
        return true;
    }

    const std::string_view termValue(term);
    return termValue == "unknown" || termValue == "dumb";
}
}

Lab12Dashboard::Lab12Dashboard(const int taskCount, const Lab12DashboardMode mode) : taskCount_(taskCount) {
    if (taskCount_ <= 0) {
        throw std::invalid_argument("lab12 dashboard requires at least one task");
    }

    interactive_ = (mode == Lab12DashboardMode::Ncurses) && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    if (interactive_) {
        if (needsDefaultTerm()) {
            setenv("TERM", "xterm-256color", 1);
        }

        try {
            initializeCurses();
            buildLayout();
            logSystem("Lab 12 ncurses dashboard online.");
        } catch (const std::exception& error) {
            if (cursesReady_) {
                endwin();
                cursesReady_ = false;
            }

            interactive_ = false;
            std::printf("[dashboard] %s Falling back to transcript mode.\n", error.what());
        }
    } else if (mode == Lab12DashboardMode::Ncurses) {
        std::printf("[dashboard] ncurses requested but no compatible TTY was detected. ");
        std::printf("Using transcript mode instead.\n");
    }
}

Lab12Dashboard::~Lab12Dashboard() {
    if (cursesReady_) {
        std::lock_guard<std::mutex> guard(renderMutex_);
        for (auto& pane : taskPanes_) {
            destroyPane(pane);
        }
        destroyPane(statePane_);
        destroyPane(systemPane_);
        endwin();
        cursesReady_ = false;
    }
}

bool Lab12Dashboard::isInteractive() const {
    return interactive_;
}

void Lab12Dashboard::logSystem(const std::string& message) {
    if (!interactive_) {
        std::printf("[system] %s\n", message.c_str());
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    appendLine(systemPane_, message);
}

void Lab12Dashboard::logTask(const int taskId, const std::string& message) {
    const int index = clampTaskId(taskId);

    if (!interactive_) {
        std::printf("[task %d] %s\n", index, message.c_str());
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    appendLine(taskPanes_[static_cast<std::size_t>(index)], message);
}

void Lab12Dashboard::setSchedulerState(
    const std::clock_t quantum,
    const int currentTask,
    const std::vector<std::string>& taskRows
) {
    quantum_ = quantum;
    currentTask_ = currentTask;
    taskRows_ = taskRows;

    if (!interactive_) {
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    renderStatePaneLocked();
}

void Lab12Dashboard::setSemaphoreState(
    const std::string& resourceName,
    const int value,
    const int ownerTaskId,
    const std::string& queueText
) {
    resourceName_ = resourceName;
    semaphoreValue_ = value;
    ownerTaskId_ = ownerTaskId;
    queueText_ = queueText;

    if (!interactive_) {
        return;
    }

    std::lock_guard<std::mutex> guard(renderMutex_);
    renderStatePaneLocked();
}

void Lab12Dashboard::finish(const std::string& message) {
    logSystem(message);

    if (interactive_) {
        std::lock_guard<std::mutex> guard(renderMutex_);
        napms(kFinishPauseMs);
    }
}

void Lab12Dashboard::initializeCurses() {
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

void Lab12Dashboard::buildLayout() {
    const int totalRows = LINES;
    const int totalCols = COLS;
    const int bodyTop = kHeaderHeight + kStateHeight;
    const int bodyHeight = totalRows - bodyTop;
    const int preferredCols = taskCount_ <= 3 ? taskCount_ : 2;
    const int taskCols = std::max(1, std::min(preferredCols, std::max(1, totalCols / kMinPaneWidth)));
    const int taskRows = static_cast<int>(std::ceil(static_cast<double>(taskCount_) / taskCols));

    if (bodyHeight < taskRows * kMinPaneHeight) {
        throw std::runtime_error("terminal is too small for the phase 12 ncurses layout");
    }

    systemPane_.title = "System";
    systemPane_.frame = newwin(kHeaderHeight, totalCols, 0, 0);
    systemPane_.body = derwin(systemPane_.frame, kHeaderHeight - 2, totalCols - 2, 1, 1);

    statePane_.title = "State Snapshot";
    statePane_.frame = newwin(kStateHeight, totalCols, kHeaderHeight, 0);
    statePane_.body = derwin(statePane_.frame, kStateHeight - 2, totalCols - 2, 1, 1);

    renderTitle(systemPane_);
    renderTitle(statePane_);
    scrollok(systemPane_.body, TRUE);

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

    renderStatePaneLocked();
}

void Lab12Dashboard::destroyPane(Pane& pane) {
    if (pane.body != nullptr) {
        delwin(pane.body);
        pane.body = nullptr;
    }

    if (pane.frame != nullptr) {
        delwin(pane.frame);
        pane.frame = nullptr;
    }
}

void Lab12Dashboard::appendLine(Pane& pane, const std::string& message) {
    renderTitle(pane);
    wprintw(pane.body, "%s\n", message.c_str());
    wrefresh(pane.body);
}

void Lab12Dashboard::renderTitle(Pane& pane) {
    werase(pane.frame);
    box(pane.frame, 0, 0);
    mvwprintw(pane.frame, 0, 2, " %s ", pane.title.c_str());
    wrefresh(pane.frame);
}

void Lab12Dashboard::renderStatePaneLocked() {
    renderTitle(statePane_);
    werase(statePane_.body);

    mvwprintw(
        statePane_.body,
        0,
        0,
        "Quantum: %lld | Current Task: %d",
        static_cast<long long>(quantum_),
        currentTask_
    );

    const std::string semaphoreLine =
        "Semaphore: " + resourceName_ +
        " value=" + std::to_string(semaphoreValue_) +
        " owner=" + std::to_string(ownerTaskId_) +
        " queue=" + queueText_;
    mvwprintw(statePane_.body, 1, 0, "%s", semaphoreLine.c_str());
    mvwprintw(statePane_.body, 2, 0, "Tasks:");

    const int maxTaskLines = kStateHeight - 5;
    for (int i = 0; i < std::min(static_cast<int>(taskRows_.size()), maxTaskLines); ++i) {
        mvwprintw(statePane_.body, 3 + i, 0, "%s", taskRows_[static_cast<std::size_t>(i)].c_str());
    }

    wrefresh(statePane_.body);
}

int Lab12Dashboard::clampTaskId(const int taskId) const {
    return std::clamp(taskId, 0, taskCount_ - 1);
}
