#ifndef EXTRA_CREDIT_PHASE10_DASHBOARD_H
#define EXTRA_CREDIT_PHASE10_DASHBOARD_H

#include <curses.h>

#include <mutex>
#include <string>
#include <vector>

enum class DashboardMode {
    Auto,
    Text,
    Ncurses,
};

class TaskDashboard {
public:
    struct DashboardLabels {
        std::string systemTitle = "Lab Dashboard";
        std::string systemSubtitle = "Modeled after the Lab 4 / Lab 7 ncurses process-window layout.";
        std::string logTitle = "Log Window";
        std::string sharedTitle = "Shared State";
        std::string consoleTitle = "Console";
        std::vector<std::string> taskTitles;
    };

    TaskDashboard(int taskCount, DashboardMode mode);
    TaskDashboard(int taskCount, DashboardMode mode, DashboardLabels labels);
    ~TaskDashboard();

    void logSystem(const std::string& message);
    void logQueue(const std::string& message);
    void logConsole(const std::string& message);
    void logTask(int taskId, const std::string& message);
    void sendTaskMessage(int fromTaskId, int toTaskId, const std::string& message);
    bool isInteractive() const;
    std::string promptInput(const std::string& prompt, int maxLength = 255);
    void setHoldOnExit(bool holdOnExit);
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
    void renderHeader();
    void renderTitle(LogPane& pane);
    void refreshConsole();
    int clampTaskId(int taskId) const;

    bool interactive_ = false;
    bool cursesReady_ = false;
    bool holdOnExit_ = true;
    int taskCount_ = 0;
    DashboardLabels labels_;
    std::mutex renderMutex_;
    LogPane headerPane_;
    LogPane systemPane_;
    LogPane queuePane_;
    LogPane consolePane_;
    std::vector<LogPane> taskPanes_;
};

#endif
