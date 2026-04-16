#include <cstdarg>
#include <iostream>
#include <ncurses.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <unistd.h>

// System constants matching the requested output
const int BUFFER_SIZE = 3;
const int PRODUCE_COUNT = 10;
const int PROD_SLEEP_US = 100000; // 0.10s
const int CONS_SLEEP_US = 850000; // 0.85s

// Shared circular buffer state
int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;
int count = 0;

// POSIX synchronization primitives
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

// Ncurses display lock and windows
pthread_mutex_t term_display_lock = PTHREAD_MUTEX_INITIALIZER;
WINDOW* dash_win = nullptr;
WINDOW* buf_win = nullptr;
WINDOW* prod_win = nullptr;
WINDOW* cons_win = nullptr;

void print_win(WINDOW* win, const char* fmt, ...) {
    pthread_mutex_lock(&term_display_lock);

    va_list args;
    va_start(args, fmt);
    vw_printw(win, fmt, args);
    va_end(args);

    wrefresh(win);
    pthread_mutex_unlock(&term_display_lock);
}

std::string get_buffer_items() {
    std::stringstream ss;
    ss << "[";

    int temp_out = out;
    for (int i = 0; i < count; ++i) {
        ss << buffer[temp_out];
        if (i < count - 1) {
            ss << ", ";
        }
        temp_out = (temp_out + 1) % BUFFER_SIZE;
    }

    ss << "]";
    return ss.str();
}

void* producer(void* arg) {
    (void)arg;
    print_win(prod_win, "Online and ready to produce.\n\n");

    for (int i = 0; i < PRODUCE_COUNT; ++i) {
        pthread_mutex_lock(&mutex);

        while (count == BUFFER_SIZE) {
            print_win(prod_win, "Buffer full. Waiting on not_full.\n\n");
            print_win(
                buf_win,
                "Producer blocked | count=%d/%d in=%d out=%d | items=%s\n\n",
                count,
                BUFFER_SIZE,
                in,
                out,
                get_buffer_items().c_str()
            );
            pthread_cond_wait(&not_full, &mutex);
            print_win(prod_win, "Wake-up received; recheck buffer.\n\n");
        }

        buffer[in] = i;
        print_win(prod_win, "Produced item %d into slot %d.\n\n", i, in);
        in = (in + 1) % BUFFER_SIZE;
        ++count;

        print_win(
            buf_win,
            "Producer inserted item %d | count=%d/%d in=%d out=%d | items=%s\n\n",
            i,
            count,
            BUFFER_SIZE,
            in,
            out,
            get_buffer_items().c_str()
        );

        print_win(prod_win, "To task 1: not_empty signaled.\n");
        print_win(dash_win, "From task 0: not_empty signaled.\n\ntask 0 -> task 1 | not_empty signaled.\n\n");

        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);

        print_win(prod_win, "Simulating I/O for 0.10s.\n");
        usleep(PROD_SLEEP_US);
        print_win(prod_win, "I/O interval finished.\n");
    }

    print_win(prod_win, "All items produced.\n");
    return nullptr;
}

void* consumer(void* arg) {
    (void)arg;
    print_win(cons_win, "Online and ready to consume.\n");

    for (int i = 0; i < PRODUCE_COUNT; ++i) {
        pthread_mutex_lock(&mutex);

        while (count == 0) {
            print_win(cons_win, "Buffer empty. Waiting on not_empty.\n\n");
            pthread_cond_wait(&not_empty, &mutex);
            print_win(cons_win, "Wake-up received; recheck buffer.\n\n");
        }

        const int item = buffer[out];
        print_win(cons_win, "Consumed item %d from slot %d.\n\n", item, out);
        out = (out + 1) % BUFFER_SIZE;
        --count;

        print_win(
            buf_win,
            "Consumer removed item %d | count=%d/%d in=%d out=%d | items=%s\n\n",
            item,
            count,
            BUFFER_SIZE,
            in,
            out,
            get_buffer_items().c_str()
        );

        print_win(cons_win, "To task 0: not_full signaled.\n");
        print_win(dash_win, "From task 1: not_full signaled.\n\ntask 1 -> task 0 | not_full signaled.\n\n");

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);

        print_win(cons_win, "Simulating I/O for 0.85s.\n");
        usleep(CONS_SLEEP_US);
        print_win(cons_win, "I/O interval finished.\n");
    }

    print_win(cons_win, "All items consumed.\n");
    return nullptr;
}

int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    int max_y = 0;
    int max_x = 0;
    getmaxyx(stdscr, max_y, max_x);

    const int dash_h = max_y / 3;
    const int buf_h = max_y / 3;
    const int task_h = max_y - dash_h - buf_h;
    const int col_width = max_x / 2;

    WINDOW* dash_frame = newwin(dash_h, max_x, 0, 0);
    box(dash_frame, 0, 0);
    mvwprintw(dash_frame, 0, 2, " Phase 11 Producer/Consumer ");
    wrefresh(dash_frame);
    dash_win = newwin(dash_h - 2, max_x - 2, 1, 1);
    scrollok(dash_win, TRUE);

    WINDOW* buf_frame = newwin(buf_h, max_x, dash_h, 0);
    box(buf_frame, 0, 0);
    mvwprintw(buf_frame, 0, 2, " Buffer State ");
    wrefresh(buf_frame);
    buf_win = newwin(buf_h - 2, max_x - 2, dash_h + 1, 1);
    scrollok(buf_win, TRUE);

    WINDOW* prod_frame = newwin(task_h, col_width, dash_h + buf_h, 0);
    box(prod_frame, 0, 0);
    mvwprintw(prod_frame, 0, 2, " Producer ");
    wrefresh(prod_frame);
    prod_win = newwin(task_h - 2, col_width - 2, dash_h + buf_h + 1, 1);
    scrollok(prod_win, TRUE);

    WINDOW* cons_frame = newwin(task_h, max_x - col_width, dash_h + buf_h, col_width);
    box(cons_frame, 0, 0);
    mvwprintw(cons_frame, 0, 2, " Consumer ");
    wrefresh(cons_frame);
    cons_win = newwin(task_h - 2, max_x - col_width - 2, dash_h + buf_h + 1, col_width + 1);
    scrollok(cons_win, TRUE);

    print_win(dash_win, "ncurses dashboard online.\n\n");
    print_win(dash_win, "Launching Lab 11 producer/consumer dashboard.\n\n");
    print_win(dash_win, "Each task has its own window. The shared pane shows the circular buffer.\n\n");
    print_win(
        dash_win,
        "BUFFER_SIZE=%d, PRODUCE_COUNT=%d, producer_sleep=%.2fs, consumer_sleep=%.2fs\n\n",
        BUFFER_SIZE,
        PRODUCE_COUNT,
        PROD_SLEEP_US / 1000000.0,
        CONS_SLEEP_US / 1000000.0
    );

    print_win(
        buf_win,
        "Initial buffer state | count=%d/%d in=%d out=%d | items=%s\n\n",
        count,
        BUFFER_SIZE,
        in,
        out,
        get_buffer_items().c_str()
    );

    pthread_t prod_thread;
    pthread_t cons_thread;

    pthread_create(&prod_thread, nullptr, producer, nullptr);
    pthread_create(&cons_thread, nullptr, consumer, nullptr);

    print_win(dash_win, "Parent waits for producer and consumer to finish.\n\n");

    pthread_join(prod_thread, nullptr);
    pthread_join(cons_thread, nullptr);

    print_win(dash_win, "All routines concluded. Press any key to exit Ncurses.\n");

    nodelay(dash_win, FALSE);
    wgetch(dash_win);

    delwin(dash_win);
    delwin(dash_frame);
    delwin(buf_win);
    delwin(buf_frame);
    delwin(prod_win);
    delwin(prod_frame);
    delwin(cons_win);
    delwin(cons_frame);
    endwin();

    return 0;
}
