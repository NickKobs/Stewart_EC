#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lab10_dashboard.h"

namespace {
constexpr int kFilesPane = 0;
constexpr int kCppPane = 1;
constexpr int kCPane = 2;
constexpr useconds_t kOperationPauseUs = 180000;

TaskDashboard* g_dashboard = nullptr;

TaskDashboard& dashboard() {
    if (g_dashboard == nullptr) {
        throw std::logic_error("lab13 dashboard is not initialized");
    }

    return *g_dashboard;
}

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    if (const char* envMode = std::getenv("LAB13_UI")) {
        const std::string_view mode(envMode);
        if (mode == "text") {
            return DashboardMode::Text;
        }
        if (mode == "ncurses") {
            return DashboardMode::Ncurses;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--text") {
            return DashboardMode::Text;
        }
        if (arg == "--ncurses") {
            return DashboardMode::Ncurses;
        }
    }

    return DashboardMode::Ncurses;
}

bool determineHoldOnExit(int argc, char* argv[]) {
    if (const char* envHold = std::getenv("LAB13_HOLD")) {
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

void pauseBriefly() {
    usleep(kOperationPauseUs);
}

void refreshTrackedFile(const std::string& filename) {
    dashboard().logTask(kFilesPane, displayName(filename) + " => " + summarizeFile(filename));
    pauseBriefly();
}

void logCpp(const std::string& message) {
    dashboard().logTask(kCppPane, message);
}

void logC(const std::string& message) {
    dashboard().logTask(kCPane, message);
}

void write_characters_Cpp(const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        logCpp("Error opening file for writing.");
        return;
    }

    logCpp("C++ Version:");
    logCpp("\tWriting characters A to E to file...");
    for (char c = 'A'; c <= 'E'; ++c) {
        outFile << '\t' << c << '\n';
    }

    outFile.close();
    refreshTrackedFile(filename);
}

void read_characters_Cpp(const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile) {
        logCpp("Error opening file for reading.");
        return;
    }

    char ch = '\0';
    logCpp("C++ Version:");
    logCpp("\tReading characters from file:");
    while (inFile >> ch) {
        logCpp(std::string("\t") + ch);
    }

    inFile.close();
    refreshTrackedFile(filename);
}

void write_numbers_Cpp(const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        logCpp("Error opening file for writing.");
        return;
    }

    logCpp("C++ Version:");
    logCpp("\tWriting numbers 1 to 5 to file...");
    for (int i = 1; i <= 5; ++i) {
        outFile << '\t' << i << '\n';
    }

    outFile.close();
    refreshTrackedFile(filename);
}

void read_numbers_Cpp(const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile) {
        logCpp("Error opening file for reading.");
        return;
    }

    int number = 0;
    logCpp("C++ Version:");
    logCpp("\tReading numbers from file:");
    while (inFile >> number) {
        logCpp("\t" + std::to_string(number));
    }

    inFile.close();
    refreshTrackedFile(filename);
}

void write_characters_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "w");
    if (file == nullptr) {
        logC("Error opening file for writing.");
        return;
    }

    logC("C Version:");
    logC("\tWriting characters A to E to file...");
    for (char c = 'A'; c <= 'E'; ++c) {
        std::fprintf(file, "\t%c\n", c);
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}

void read_characters_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "r");
    if (file == nullptr) {
        logC("Error opening file for reading.");
        return;
    }

    char ch = '\0';
    logC("C Version:");
    logC("\tReading characters from file:");
    while (std::fscanf(file, " %c", &ch) != EOF) {
        logC(std::string("\t") + ch);
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}

void write_numbers_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "w");
    if (file == nullptr) {
        logC("Error opening file for writing.");
        return;
    }

    logC("C Version:");
    logC("\tWriting numbers 1 to 5 to file...");
    for (int i = 1; i <= 5; ++i) {
        std::fprintf(file, "\t%d\n", i);
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}

void read_numbers_C(const char* filename) {
    std::FILE* file = std::fopen(filename, "r");
    if (file == nullptr) {
        logC("Error opening file for reading.");
        return;
    }

    int number = 0;
    logC("C Version:");
    logC("\tReading numbers from file:");
    while (std::fscanf(file, "%d", &number) != EOF) {
        logC("\t" + std::to_string(number));
    }

    std::fclose(file);
    refreshTrackedFile(filename);
}
}

int main(int argc, char* argv[]) {
    try {
        TaskDashboard::DashboardLabels labels;
        labels.systemTitle = "Lab 13 Sequential I/O";
        labels.systemSubtitle =
            "Lab 4 / Lab 7 layout for tracked files, C++ streams, and C stdio traces.";
        labels.sharedTitle = "Run Notes";
        labels.taskTitles = {"Tracked Files", "C++ Streams", "C stdio"};

        TaskDashboard activeDashboard(3, determineDashboardMode(argc, argv), labels);
        activeDashboard.setHoldOnExit(determineHoldOnExit(argc, argv));
        g_dashboard = &activeDashboard;

        const std::string charactersCppFile = "Lab_13/characters_Cpp.txt";
        const std::string numbersCppFile = "Lab_13/numbers_Cpp.txt";
        const char* charactersCFile = "Lab_13/characters_C.txt";
        const char* numbersCFile = "Lab_13/numbers_C.txt";

        dashboard().logSystem("Sequential access I/O lab started.");
        dashboard().logSystem("Generated submission files are written under Lab_13/.");
        dashboard().logSystem("The four lab routines now render through the shared Lab 4 / Lab 7 dashboard.");
        dashboard().logQueue("Tracked files begin empty or missing until each routine writes them.");

        write_characters_Cpp(charactersCppFile);
        read_characters_Cpp(charactersCppFile);
        write_numbers_Cpp(numbersCppFile);
        read_numbers_Cpp(numbersCppFile);

        dashboard().logQueue("C++ stream pass complete. Moving to the C stdio pass.");

        write_characters_C(charactersCFile);
        read_characters_C(charactersCFile);
        write_numbers_C(numbersCFile);
        read_numbers_C(numbersCFile);

        dashboard().finish("Lab 13 sequential I/O run complete.");
        g_dashboard = nullptr;
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "lab13 error: %s\n", error.what());
        return 1;
    }
}
