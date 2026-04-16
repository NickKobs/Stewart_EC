#include <cstdarg>
#include <iostream>
#include <ncurses.h>
#include <pthread.h>
#include <queue>
#include <string>
#include <unistd.h>

const int MAX_TASKS = 3;
const int QUANTUM_START = 300;
const int QUANTUM_RUN = 333;
const int CPU_WORK = 720;

enum State { DEAD, READY, RUNNING, BLOCKED };

std::string state_to_str(const State state) {
    switch (state) {
        case DEAD:
            return "DEAD";
        case READY:
            return "READY";
        case RUNNING:
            return "RUNNING";
        case BLOCKED:
            return "BLOCKED";
        default:
            return "UNKNOWN";
    }
}

struct Task {
    int id;
    State state;
    int elapsed;
};

struct Semaphore {
    std::string name;
    int value;
    int owner;
    std::queue<int> wait_queue;
};

Task tasks[MAX_TASKS];
Semaphore resource1 = {"resource1", 1, -1, {}};
int current_task = -1;
int quantum = QUANTUM_START;

pthread_mutex_t term_display_lock = PTHREAD_MUTEX_INITIALIZER;
WINDOW* sys_win = nullptr;
WINDOW* snap_win = nullptr;
WINDOW* task_wins[MAX_TASKS] = {nullptr};

void print_win(WINDOW* win, const char* fmt, ...) {
    pthread_mutex_lock(&term_display_lock);
    va_list args;
    va_start(args, fmt);
    vw_printw(win, fmt, args);
    va_end(args);
    wrefresh(win);
    pthread_mutex_unlock(&term_display_lock);
    usleep(80000);
}

void print_snapshot() {
    pthread_mutex_lock(&term_display_lock);

    std::string queue_text = "[";
    if (resource1.wait_queue.empty()) {
        queue_text += "empty]";
    } else {
        std::queue<int> temp = resource1.wait_queue;
        bool first = true;
        while (!temp.empty()) {
            if (!first) {
                queue_text += ", ";
            }
            queue_text += std::to_string(temp.front());
            temp.pop();
            first = false;
        }
        queue_text += "]";
    }

    werase(snap_win);
    wprintw(snap_win, "Quantum: %d | Current Task: %d\n", quantum, current_task);
    wprintw(
        snap_win,
        "Semaphore: %s value=%d owner=%d queue=%s\n",
        resource1.name.c_str(),
        resource1.value,
        resource1.owner,
        queue_text.c_str()
    );
    wprintw(snap_win, "Tasks:\n");

    for (int i = 0; i < MAX_TASKS; ++i) {
        wprintw(
            snap_win,
            "Task %d | %s | elapsed=%d",
            tasks[i].id == -1 ? -1 : i,
            state_to_str(tasks[i].state).c_str(),
            tasks[i].elapsed
        );

        if (i == current_task) {
            wprintw(snap_win, " | CURRENT");
        }
        wprintw(snap_win, "\n");
    }

    wrefresh(snap_win);
    pthread_mutex_unlock(&term_display_lock);
    usleep(100000);
}

void simulate_cpu() {
    print_win(sys_win, "Simulating CPU work.\n\n");
    for (int i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state != DEAD) {
            tasks[i].elapsed += CPU_WORK;
        }
    }
}

void attempt_yield(const int task_id) {
    print_win(task_wins[task_id], "Attempting to yield.\n\n");
    print_win(sys_win, "Task %d yield check: elapsed=%d quantum=%d\n", task_id, tasks[task_id].elapsed, quantum);

    if (tasks[task_id].elapsed >= quantum) {
        print_win(sys_win, "Quantum expired. Looking for the next READY task.\n\n");

        int next_task = (task_id + 1) % MAX_TASKS;
        while (tasks[next_task].state != READY && next_task != task_id) {
            next_task = (next_task + 1) % MAX_TASKS;
        }

        print_win(sys_win, "Switching from task %d to task %d.\n\n", task_id, next_task);

        if (tasks[task_id].state == RUNNING) {
            tasks[task_id].state = READY;
        }

        tasks[next_task].state = RUNNING;
        tasks[next_task].elapsed = 0;
        current_task = next_task;

        print_win(task_wins[next_task], "Scheduled RUNNING.\n\n");
    } else {
        print_win(sys_win, "Yield ignored because quantum remains.\n\n");
    }
}

void sem_wait(const int task_id) {
    print_win(sys_win, "Trying to obtain semaphore %s.\n", resource1.name.c_str());
    print_win(task_wins[task_id], "Requesting semaphore %s.\n\n", resource1.name.c_str());

    if (resource1.value > 0) {
        resource1.value--;
        resource1.owner = task_id;
        print_win(task_wins[task_id], "Obtained semaphore %s.\n\n", resource1.name.c_str());
        print_win(sys_win, "Task %d obtained %s.\n\n", task_id, resource1.name.c_str());
    } else {
        print_win(task_wins[task_id], "Blocked and queued for %s.\n\n", resource1.name.c_str());
        print_win(sys_win, "Task %d blocked on %s.\n\n", task_id, resource1.name.c_str());
        tasks[task_id].state = BLOCKED;
        tasks[task_id].elapsed = 0;
        resource1.wait_queue.push(task_id);
    }
}

void sem_release(const int task_id) {
    print_win(sys_win, "Trying to release semaphore %s.\n\n", resource1.name.c_str());
    print_win(sys_win, "Semaphore release check: task=%d owner=%d\n\n", task_id, resource1.owner);

    if (resource1.owner == task_id) {
        if (!resource1.wait_queue.empty()) {
            const int next_owner = resource1.wait_queue.front();
            resource1.wait_queue.pop();
            resource1.owner = next_owner;
            tasks[next_owner].state = READY;
            print_win(sys_win, "Unblock task %d from %s.\n\n", next_owner, resource1.name.c_str());
            print_win(task_wins[next_owner], "Woken from semaphore queue and moved to READY.\n\n");
        } else {
            resource1.value++;
            resource1.owner = -1;
        }
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    int max_y = 0;
    int max_x = 0;
    getmaxyx(stdscr, max_y, max_x);

    const int sys_h = max_y / 3;
    const int snap_h = max_y / 3;
    const int task_h = max_y - sys_h - snap_h;
    const int col_width = max_x / 3;

    WINDOW* sys_f = newwin(sys_h, max_x, 0, 0);
    box(sys_f, 0, 0);
    mvwprintw(sys_f, 0, 2, " System ");
    wrefresh(sys_f);

    WINDOW* snap_f = newwin(snap_h, max_x, sys_h, 0);
    box(snap_f, 0, 0);
    mvwprintw(snap_f, 0, 2, " State Snapshot ");
    wrefresh(snap_f);

    WINDOW* t0_f = newwin(task_h, col_width, sys_h + snap_h, 0);
    box(t0_f, 0, 0);
    mvwprintw(t0_f, 0, 2, " Task 0 ");
    wrefresh(t0_f);

    WINDOW* t1_f = newwin(task_h, col_width, sys_h + snap_h, col_width);
    box(t1_f, 0, 0);
    mvwprintw(t1_f, 0, 2, " Task 1 ");
    wrefresh(t1_f);

    WINDOW* t2_f = newwin(task_h, max_x - 2 * col_width, sys_h + snap_h, 2 * col_width);
    box(t2_f, 0, 0);
    mvwprintw(t2_f, 0, 2, " Task 2 ");
    wrefresh(t2_f);

    sys_win = newwin(sys_h - 2, max_x - 2, 1, 1);
    scrollok(sys_win, TRUE);
    snap_win = newwin(snap_h - 2, max_x - 2, sys_h + 1, 1);
    scrollok(snap_win, TRUE);
    task_wins[0] = newwin(task_h - 2, col_width - 2, sys_h + snap_h + 1, 1);
    scrollok(task_wins[0], TRUE);
    task_wins[1] = newwin(task_h - 2, col_width - 2, sys_h + snap_h + 1, col_width + 1);
    scrollok(task_wins[1], TRUE);
    task_wins[2] = newwin(task_h - 2, max_x - 2 * col_width - 2, sys_h + snap_h + 1, 2 * col_width + 1);
    scrollok(task_wins[2], TRUE);

    for (int i = 0; i < MAX_TASKS; ++i) {
        tasks[i] = {-1, DEAD, 0};
    }

    print_win(sys_win, "Lab 12 ncurses dashboard online.\n\n");
    print_snapshot();

    print_win(sys_win, "Step 1 & 2: Create a scheduler and attempt to create 4 tasks.\n\n");

    print_win(sys_win, "Creating task # 0\n");
    tasks[0] = {0, READY, 0};
    print_win(task_wins[0], "Created and placed in READY state.\n\n");
    print_snapshot();

    print_win(sys_win, "Creating task # 1\n");
    tasks[1] = {1, READY, 0};
    print_win(task_wins[1], "Created and placed in READY state.\n\n");
    print_snapshot();

    print_win(sys_win, "Creating task # 2\n");
    tasks[2] = {2, READY, 0};
    print_win(task_wins[2], "Created and placed in READY state.\n\n");
    print_snapshot();

    print_win(sys_win, "Create_task() FAILED: Available tasks exceeded.  MAX_TASKS = 3\n\n");
    print_snapshot();

    print_win(sys_win, "Step 3: Start the scheduler.\n\n");
    print_win(sys_win, "Scheduling started.\n\n");
    quantum = QUANTUM_RUN;
    current_task = 0;
    tasks[0].state = RUNNING;
    print_snapshot();

    print_win(task_wins[0], "Started running.\n\n");
    print_snapshot();
    print_snapshot();

    print_win(sys_win, "Step 4: Try some context switching (3 yields).\n");

    simulate_cpu();
    print_snapshot();
    attempt_yield(0);
    print_snapshot();
    print_snapshot();

    simulate_cpu();
    print_snapshot();
    attempt_yield(1);
    print_snapshot();
    print_snapshot();

    simulate_cpu();
    print_snapshot();
    attempt_yield(2);
    print_snapshot();
    print_snapshot();

    print_win(sys_win, "Step 5: Test the scheduler and semaphore together.\n\n");
    print_snapshot();
    print_snapshot();

    sem_wait(0);
    print_snapshot();
    print_snapshot();

    simulate_cpu();
    print_snapshot();
    attempt_yield(0);
    print_snapshot();
    print_snapshot();

    sem_wait(1);
    print_snapshot();
    attempt_yield(1);
    print_snapshot();
    print_snapshot();
    print_snapshot();

    simulate_cpu();
    print_snapshot();
    attempt_yield(1);
    print_snapshot();
    print_snapshot();

    sem_wait(2);
    print_snapshot();
    attempt_yield(2);
    print_snapshot();
    print_snapshot();
    print_snapshot();

    simulate_cpu();
    print_snapshot();
    attempt_yield(2);
    print_snapshot();
    print_snapshot();

    sem_release(0);
    print_snapshot();
    attempt_yield(0);
    print_snapshot();
    print_snapshot();
    print_snapshot();

    simulate_cpu();
    print_snapshot();
    attempt_yield(0);
    print_snapshot();
    print_snapshot();

    print_win(sys_win, "Lab 12 run complete. Closing dashboard shortly.\n\n");
    print_win(sys_win, "Exiting Scheduler.....\n\n");

    nodelay(sys_win, FALSE);
    wgetch(sys_win);

    delwin(sys_win);
    delwin(sys_f);
    delwin(snap_win);
    delwin(snap_f);
    for (auto*& task_win : task_wins) {
        delwin(task_win);
    }
    delwin(t0_f);
    delwin(t1_f);
    delwin(t2_f);
    endwin();

    return 0;
}
