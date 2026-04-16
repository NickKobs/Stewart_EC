#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <curses.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

enum class Lab13DashboardMode {
    Text,
    Ncurses,
};

Lab13DashboardMode determineDashboardMode(int argc, char* argv[]);
bool determineHoldOnExit(int argc, char* argv[]);

void write_characters_C(const char* filename);
void read_characters_C(const char* filename);
void write_numbers_C(const char* filename);
void read_numbers_C(const char* filename);

void write_characters_Cpp(const std::string& filename);
void read_characters_Cpp(const std::string& filename);
void write_numbers_Cpp(const std::string& filename);
void read_numbers_Cpp(const std::string& filename);

namespace {
constexpr int kHeaderHeight = 6;
constexpr int kFileStateHeight = 8;
constexpr int kMinBodyHeight = 8;
constexpr int kMinTerminalWidth = 72;
constexpr int kFinishPauseMs = 1200;
constexpr int kOperationPauseMs = 180;

bool needsDefaultTerm() {
    const char* term = std::getenv("TERM");
    if (term == nullptr || term[0] == '\0') {
        return true;
    }

    const std::string_view value(term);
    return value == "unknown" || value == "dumb";
}

std::string displayName(const std::string& filename) {
    return std::filesystem::path(filename).filename().string();
}

std::string summarizeFile(const std::string& filename) {
    std::ifstream input(filename);
    if (!input) {
        return "[missing]";
    }

    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) {
        return "[empty]";
    }

    std::string joined;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            joined += ' ';
        }
        joined += tokens[i];
    }

    return joined;
}

class Lab13Dashboard {
public:
    Lab13Dashboard(const Lab13DashboardMode mode, const bool holdOnExit) : holdOnExit_(holdOnExit) {
        interactive_ = (mode == Lab13DashboardMode::Ncurses) && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
        if (interactive_) {
            if (needsDefaultTerm()) {
                setenv("TERM", "xterm-256color", 1);
            }

            try {
                initializeCurses();
                buildLayout();
                logSystem("Lab 13 ncurses dashboard online.");
            } catch (const std::exception& error) {
                if (cursesReady_) {
                    endwin();
                    cursesReady_ = false;
                }

                interactive_ = false;
                std::printf("[dashboard] %s Falling back to transcript mode.\n", error.what());
            }
        } else if (mode == Lab13DashboardMode::Ncurses) {
            std::printf("[dashboard] ncurses requested but no compatible TTY was detected. ");
            std::printf("Using transcript mode instead.\n");
        }
    }

    ~Lab13Dashboard() {
        if (!cursesReady_) {
            return;
        }

        destroyPane(cPane_);
        destroyPane(cppPane_);
        destroyPane(filesPane_);
        destroyPane(systemPane_);
        endwin();
        cursesReady_ = false;
    }

    void setTrackedFiles(const std::vector<std::string>& files) {
        trackedFiles_ = files;
        trackedFileSummaries_.resize(trackedFiles_.size(), "[missing]");
        for (std::size_t i = 0; i < trackedFiles_.size(); ++i) {
            trackedFileSummaries_[i] = summarizeFile(trackedFiles_[i]);
        }

        if (interactive_) {
            renderFilesPane();
        }
    }

    void logSystem(const std::string& message) {
        appendLine(systemPane_, "[system] " + message, false);
    }

    void logCpp(const std::string& message) {
        appendLine(cppPane_, message, true);
    }

    void logC(const std::string& message) {
        appendLine(cPane_, message, true);
    }

    void refreshFileSnapshot(const std::string& filename) {
        const auto it = std::find(trackedFiles_.begin(), trackedFiles_.end(), filename);
        if (it == trackedFiles_.end()) {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(std::distance(trackedFiles_.begin(), it));
        trackedFileSummaries_[index] = summarizeFile(filename);

        if (!interactive_) {
            std::printf(
                "[file] %s => %s\n",
                displayName(filename).c_str(),
                trackedFileSummaries_[index].c_str()
            );
            return;
        }

        renderFilesPane();
    }

    void pulse() const {
        if (interactive_) {
            napms(kOperationPauseMs);
        }
    }

    void finish(const std::string& message) {
        logSystem(message);

        if (!interactive_) {
            return;
        }

        if (holdOnExit_) {
            appendLine(systemPane_, "[system] Press any key to close the dashboard.", false);
            wgetch(systemPane_.body);
            return;
        }

        napms(kFinishPauseMs);
    }

private:
    struct Pane {
        WINDOW* frame = nullptr;
        WINDOW* body = nullptr;
        std::string title;
    };

    void initializeCurses() {
        if (initscr() == nullptr) {
            throw std::runtime_error("failed to initialize ncurses");
        }

        cursesReady_ = true;
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        nodelay(stdscr, FALSE);
        curs_set(0);
        refresh();
    }

    void buildLayout() {
        if (LINES < kHeaderHeight + kFileStateHeight + kMinBodyHeight || COLS < kMinTerminalWidth) {
            throw std::runtime_error("terminal is too small for the Lab 13 layout");
        }

        const int bodyY = kHeaderHeight + kFileStateHeight;
        const int bodyHeight = LINES - bodyY;
        const int leftWidth = COLS / 2;
        const int rightWidth = COLS - leftWidth;

        systemPane_.title = "System";
        filesPane_.title = "Tracked Files";
        cppPane_.title = "C++ Streams";
        cPane_.title = "C stdio";

        systemPane_.frame = newwin(kHeaderHeight, COLS, 0, 0);
        systemPane_.body = derwin(systemPane_.frame, kHeaderHeight - 2, COLS - 2, 1, 1);

        filesPane_.frame = newwin(kFileStateHeight, COLS, kHeaderHeight, 0);
        filesPane_.body = derwin(filesPane_.frame, kFileStateHeight - 2, COLS - 2, 1, 1);

        cppPane_.frame = newwin(bodyHeight, leftWidth, bodyY, 0);
        cppPane_.body = derwin(cppPane_.frame, bodyHeight - 2, leftWidth - 2, 1, 1);

        cPane_.frame = newwin(bodyHeight, rightWidth, bodyY, leftWidth);
        cPane_.body = derwin(cPane_.frame, bodyHeight - 2, rightWidth - 2, 1, 1);

        renderTitle(systemPane_);
        renderTitle(filesPane_);
        renderTitle(cppPane_);
        renderTitle(cPane_);

        scrollok(systemPane_.body, TRUE);
        scrollok(cppPane_.body, TRUE);
        scrollok(cPane_.body, TRUE);
        renderFilesPane();
    }

    static void destroyPane(Pane& pane) {
        if (pane.body != nullptr) {
            delwin(pane.body);
            pane.body = nullptr;
        }

        if (pane.frame != nullptr) {
            delwin(pane.frame);
            pane.frame = nullptr;
        }
    }

    static void renderTitle(Pane& pane) {
        werase(pane.frame);
        box(pane.frame, 0, 0);
        mvwprintw(pane.frame, 0, 2, " %s ", pane.title.c_str());
        wrefresh(pane.frame);
    }

    void appendLine(Pane& pane, const std::string& message, const bool prefixTranscript) {
        if (!interactive_) {
            const char* label = "system";
            if (&pane == &cppPane_) {
                label = "cpp";
            } else if (&pane == &cPane_) {
                label = "c";
            }

            if (prefixTranscript) {
                std::printf("[%s] %s\n", label, message.c_str());
            } else {
                std::printf("%s\n", message.c_str());
            }
            return;
        }

        renderTitle(pane);
        wprintw(pane.body, "%s\n", message.c_str());
        wrefresh(pane.body);
    }

    void renderFilesPane() {
        if (!interactive_) {
            return;
        }

        renderTitle(filesPane_);
        werase(filesPane_.body);
        mvwprintw(filesPane_.body, 0, 0, "Generated sequential files under tmp/lab13:");

        for (std::size_t i = 0; i < trackedFiles_.size(); ++i) {
            const int row = static_cast<int>(i) + 2;
            if (row >= kFileStateHeight - 2) {
                break;
            }

            const std::string line =
                displayName(trackedFiles_[i]) + " => " + trackedFileSummaries_[i];
            mvwprintw(filesPane_.body, row, 0, "%s", line.c_str());
        }

        wrefresh(filesPane_.body);
    }

    bool interactive_ = false;
    bool cursesReady_ = false;
    bool holdOnExit_ = false;
    Pane systemPane_;
    Pane filesPane_;
    Pane cppPane_;
    Pane cPane_;
    std::vector<std::string> trackedFiles_;
    std::vector<std::string> trackedFileSummaries_;
};

Lab13Dashboard* g_dashboard = nullptr;

Lab13Dashboard& dashboard() {
    if (g_dashboard == nullptr) {
        throw std::logic_error("lab13 dashboard is not initialized");
    }

    return *g_dashboard;
}

void refreshTrackedFile(const std::string& filename) {
    dashboard().refreshFileSnapshot(filename);
    dashboard().pulse();
}
}

int main(int argc, char* argv[]) {
    std::filesystem::create_directories("tmp/lab13");

    Lab13Dashboard activeDashboard(determineDashboardMode(argc, argv), determineHoldOnExit(argc, argv));
    g_dashboard = &activeDashboard;

    const std::string charactersCppFile = "tmp/lab13/characters_CPP.txt";
    const std::string numbersCppFile = "tmp/lab13/numbers_CPP.txt";
    const char* charactersCFile = "tmp/lab13/characters_C.txt";
    const char* numbersCFile = "tmp/lab13/numbers_C.txt";

    activeDashboard.setTrackedFiles({charactersCppFile, numbersCppFile, charactersCFile, numbersCFile});
    activeDashboard.logSystem("Sequential Access I/O lab started.");
    activeDashboard.logSystem("Generated files are written under tmp/lab13.");
    activeDashboard.logSystem("The four lab routines are preserved and shown in ncurses when a TTY is available.");

    write_characters_Cpp(charactersCppFile);
    read_characters_Cpp(charactersCppFile);

    write_numbers_Cpp(numbersCppFile);
    read_numbers_Cpp(numbersCppFile);

    activeDashboard.logSystem("C++ stream pass complete. Moving to the C stdio pass.");

    write_characters_C(charactersCFile);
    read_characters_C(charactersCFile);

    write_numbers_C(numbersCFile);
    read_numbers_C(numbersCFile);

    activeDashboard.finish("Lab 13 sequential I/O run complete.");
    g_dashboard = nullptr;
    return 0;
}

Lab13DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB13_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "text") {
            return Lab13DashboardMode::Text;
        }
        if (mode == "ncurses") {
            return Lab13DashboardMode::Ncurses;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--text") {
            return Lab13DashboardMode::Text;
        }
        if (arg == "--ncurses") {
            return Lab13DashboardMode::Ncurses;
        }
    }

    return Lab13DashboardMode::Ncurses;
}

bool determineHoldOnExit(int argc, char* argv[]) {
    const char* envHold = std::getenv("LAB13_HOLD");
    if (envHold != nullptr) {
        const std::string_view hold(envHold);
        if (hold == "0" || hold == "false" || hold == "FALSE" || hold == "no") {
            return false;
        }
        if (hold == "1" || hold == "true" || hold == "TRUE" || hold == "yes") {
            return true;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--no-hold") {
            return false;
        }
        if (arg == "--hold") {
            return true;
        }
    }

    return true;
}

void write_characters_Cpp(const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        dashboard().logCpp("Error opening file for writing.");
        return;
    }

    dashboard().logCpp("C++ Version:");
    dashboard().logCpp("\tWriting characters A to E to file...");
    for (char c = 'A'; c <= 'E'; ++c) {
        outFile << '\t' << c << '\n';
    }

    outFile.close();
    refreshTrackedFile(filename);
}

void read_characters_Cpp(const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile) {
        dashboard().logCpp("Error opening file for reading.");
        return;
    }

    char ch = '\0';
    dashboard().logCpp("C++ Version:");
    dashboard().logCpp("\tReading characters from file:");
    while (inFile >> ch) {
        dashboard().logCpp(std::string("\t") + ch);
    }

    inFile.close();
    refreshTrackedFile(filename);
}

void write_numbers_Cpp(const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        dashboard().logCpp("Error opening file for writing.");
        return;
    }

    dashboard().logCpp("C++ Version:");
    dashboard().logCpp("\tWriting numbers 1 to 5 to file...");
    for (int i = 1; i <= 5; ++i) {
        outFile << '\t' << i << '\n';
    }

    outFile.close();
    refreshTrackedFile(filename);
}

void read_numbers_Cpp(const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile) {
        dashboard().logCpp("Error opening file for reading.");
        return;
    }

    int number = 0;
    dashboard().logCpp("C++ Version:");
    dashboard().logCpp("\tReading numbers from file:");
    while (inFile >> number) {
        dashboard().logCpp("\t" + std::to_string(number));
    }

    inFile.close();
    refreshTrackedFile(filename);
}

void write_characters_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "w");
    if (file == nullptr) {
        dashboard().logC("Error opening file for writing.");
        return;
    }

    dashboard().logC("C Version:");
    dashboard().logC("\tWriting characters A to E to file...");
    for (char c = 'A'; c <= 'E'; ++c) {
        std::fprintf(file, "\t%c\n", c);
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}

void read_characters_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "r");
    if (file == nullptr) {
        dashboard().logC("Error opening file for reading.");
        return;
    }

    char ch = '\0';
    dashboard().logC("C Version:");
    dashboard().logC("\tReading characters from file:");
    while (std::fscanf(file, " %c", &ch) != EOF) {
        dashboard().logC(std::string("\t") + ch);
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}

void write_numbers_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "w");
    if (file == nullptr) {
        dashboard().logC("Error opening file for writing.");
        return;
    }

    dashboard().logC("C Version:");
    dashboard().logC("\tWriting numbers 1 to 5 to file...");
    for (int i = 1; i <= 5; ++i) {
        std::fprintf(file, "\t%d\n", i);
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}

void read_numbers_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "r");
    if (file == nullptr) {
        dashboard().logC("Error opening file for reading.");
        return;
    }

    int number = 0;
    dashboard().logC("C Version:");
    dashboard().logC("\tReading numbers from file:");
    while (std::fscanf(file, "%d", &number) != EOF) {
        dashboard().logC("\t" + std::to_string(number));
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}
