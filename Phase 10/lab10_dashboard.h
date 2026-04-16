#ifndef EXTRA_CREDIT_PHASE10_DASHBOARD_H
#define EXTRA_CREDIT_PHASE10_DASHBOARD_H

#include <curses.h>

#include <mutex>
#include <string>
#include <vector>

class TaskDashboard {
public:
    explicit TaskDashboard(int taskCount);
    ~TaskDashboard();

    void logSystem(const std::string& message);
    void logQueue(const std::string& message);
    void logTask(int taskId, const std::string& message);
    void sendTaskMessage(int fromTaskId, int toTaskId, const std::string& message);
    void finish(const std::string& message);

private:
    struct LogPane {
        WINDOW* frame = nullptr;
        WINDOW* body = nullptr;
        std::string title;
    };

    void initializeCurses();
    void buildLayout();
    void destroyPane(LogPane& pane);
    void appendLine(LogPane& pane, const std::string& message);
    void renderTitle(LogPane& pane);
    int clampTaskId(int taskId) const;

    bool interactive_ = false;
    bool cursesReady_ = false;
    int taskCount_ = 0;
    std::mutex renderMutex_;
    LogPane systemPane_;
    LogPane queuePane_;
    std::vector<LogPane> taskPanes_;
};

#endif
