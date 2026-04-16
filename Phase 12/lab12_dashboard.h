#ifndef EXTRA_CREDIT_PHASE12_DASHBOARD_H
#define EXTRA_CREDIT_PHASE12_DASHBOARD_H

#include <curses.h>

#include <ctime>
#include <mutex>
#include <string>
#include <vector>

enum class Lab12DashboardMode {
    Text,
    Ncurses,
};

class Lab12Dashboard {
public:
    Lab12Dashboard(int taskCount, Lab12DashboardMode mode);
    ~Lab12Dashboard();

    [[nodiscard]] bool isInteractive() const;

    void logSystem(const std::string& message);
    void logTask(int taskId, const std::string& message);
    void setSchedulerState(std::clock_t quantum, int currentTask, const std::vector<std::string>& taskRows);
    void setSemaphoreState(const std::string& resourceName, int value, int ownerTaskId, const std::string& queueText);
    void finish(const std::string& message);

private:
    struct Pane {
        WINDOW* frame = nullptr;
        WINDOW* body = nullptr;
        std::string title;
    };

    void initializeCurses();
    void buildLayout();
    void destroyPane(Pane& pane);
    void appendLine(Pane& pane, const std::string& message);
    void renderTitle(Pane& pane);
    void renderStatePaneLocked();
    [[nodiscard]] int clampTaskId(int taskId) const;

    bool interactive_ = false;
    bool cursesReady_ = false;
    int taskCount_ = 0;
    std::mutex renderMutex_;
    Pane systemPane_;
    Pane statePane_;
    std::vector<Pane> taskPanes_;

    std::clock_t quantum_ = 0;
    int currentTask_ = -1;
    std::vector<std::string> taskRows_;
    std::string resourceName_ = "resource1";
    int semaphoreValue_ = 1;
    int ownerTaskId_ = -1;
    std::string queueText_ = "[empty]";
};

#endif
