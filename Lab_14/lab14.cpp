#include "lab10_dashboard.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace {
constexpr int kWritePane = 0;
constexpr int kReadPane = 1;
constexpr int kReadWritePane1 = 2;
constexpr int kReadWritePane2 = 3;

const std::filesystem::path kFile1 = "random_file_1.txt";
const std::filesystem::path kFile2 = "random_file_2.txt";

std::string escapeForLog(std::string_view text) {
    std::ostringstream escaped;
    for (const unsigned char ch : text) {
        switch (ch) {
            case '\0':
                escaped << "\\0";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                if (ch >= 32U && ch <= 126U) {
                    escaped << static_cast<char>(ch);
                } else {
                    escaped << "\\x";
                    constexpr char hex[] = "0123456789ABCDEF";
                    escaped << hex[(ch >> 4U) & 0x0FU] << hex[ch & 0x0FU];
                }
                break;
        }
    }

    return escaped.str();
}

std::string readWholeFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string readWholeStream(std::fstream& file) {
    file.clear();
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        file.clear();
        file.seekg(0, std::ios::beg);
        return {};
    }

    std::string contents(static_cast<std::size_t>(size), '\0');
    file.seekg(0, std::ios::beg);
    file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.clear();
    file.seekg(0, std::ios::beg);
    return contents;
}

std::string describeFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return path.string() + " | missing";
    }

    const std::string contents = readWholeFile(path);
    std::ostringstream summary;
    summary << path.string() << " | size=" << contents.size() << " | data=\"" << escapeForLog(contents) << '"';
    return summary.str();
}

void logSnapshots(TaskDashboard& dashboard, const std::string& label) {
    dashboard.logQueue(label);
    dashboard.logQueue(describeFile(kFile1));
    dashboard.logQueue(describeFile(kFile2));
}

void ensureFileExists(const std::filesystem::path& path) {
    std::ifstream existing(path, std::ios::binary);
    if (existing.is_open()) {
        return;
    }

    std::ofstream create(path, std::ios::binary);
}

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB14_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "ncurses") {
            return DashboardMode::Ncurses;
        }
        if (mode == "text") {
            return DashboardMode::Text;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--ncurses") {
            return DashboardMode::Ncurses;
        }
        if (arg == "--text") {
            return DashboardMode::Text;
        }
    }

    return DashboardMode::Auto;
}

bool determineHoldOnExit(int argc, char* argv[]) {
    const char* envHold = std::getenv("LAB14_HOLD");
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

void random_file_write(TaskDashboard& dashboard) {
    constexpr std::string_view initialText =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "0123456789\n"
        "abcdefghijklmnopqrstuvwxyz\n";
    constexpr std::string_view tailText = "Writing to Location 100...and beyond";

    dashboard.logSystem("Step 2: Opening random_file_1.txt for output.");
    std::fstream file(kFile1, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        dashboard.logTask(kWritePane, "Failed to open random_file_1.txt for write.");
        return;
    }

    dashboard.logTask(kWritePane, "write() emits the base alphabet payload at offset 0.");
    file.seekp(0, std::ios::beg);
    file.write(initialText.data(), static_cast<std::streamsize>(initialText.size()));

    dashboard.logTask(kWritePane, "operator<< appends a blank line and \"Another line\".");
    file << '\n';
    file << "Another line\n";

    dashboard.logTask(kWritePane, "seekp(38) + put() overwrites the first ten lowercase letters.");
    file.seekp(38, std::ios::beg);
    for (int code = 'A'; code < 'A' + 10; ++code) {
        file.put(static_cast<char>(code));
    }

    dashboard.logTask(kWritePane, "seekp(100) jumps beyond EOF and writes a tail record.");
    file.seekp(100, std::ios::beg);
    file.write(tailText.data(), static_cast<std::streamsize>(tailText.size()));
    file.flush();

    dashboard.logTask(kWritePane, "File write sequence complete.");
    file.close();
    logSnapshots(dashboard, "Snapshot after Step 2");
}

void random_file_read(TaskDashboard& dashboard) {
    dashboard.logSystem("Step 3: Opening random_file_1.txt for input.");
    std::fstream file(kFile1, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        dashboard.logTask(kReadPane, "Failed to open random_file_1.txt for read.");
        return;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    dashboard.logTask(kReadPane, "tellg() reports fileSize=" + std::to_string(fileSize) + " bytes.");

    const std::string forward = readWholeStream(file);
    dashboard.logTask(kReadPane, "Forward read: \"" + escapeForLog(forward) + '"');

    if (!forward.empty()) {
        char lastChar = '\0';
        file.clear();
        file.seekg(fileSize - 1, std::ios::beg);
        file.get(lastChar);
        dashboard.logTask(kReadPane, "Last char: \"" + escapeForLog(std::string(1, lastChar)) + '"');

        char firstChar = '\0';
        file.clear();
        file.seekg(0, std::ios::beg);
        file.get(firstChar);
        dashboard.logTask(kReadPane, "First char: \"" + escapeForLog(std::string(1, firstChar)) + '"');

        std::string backward;
        backward.reserve(forward.size());
        for (std::streamoff position = fileSize; position > 0; --position) {
            file.clear();
            file.seekg(position - 1, std::ios::beg);
            char current = '\0';
            file.get(current);
            backward.push_back(current);
        }
        dashboard.logTask(kReadPane, "Backward read: \"" + escapeForLog(backward) + '"');
    }

    file.close();
    logSnapshots(dashboard, "Snapshot after Step 3");
}

void random_file_read_write_1(TaskDashboard& dashboard) {
    dashboard.logSystem("Step 4: Opening random_file_2.txt for read/write.");
    ensureFileExists(kFile2);

    std::fstream file(kFile2, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        dashboard.logTask(kReadWritePane1, "Failed to open random_file_2.txt for read/write.");
        return;
    }

    file.seekp(0, std::ios::beg);
    file << "Record 01.";
    dashboard.logTask(kReadWritePane1, "Wrote \"Record 01.\" at offset 0.");

    file.seekp(20, std::ios::beg);
    file << "Record 03.";
    dashboard.logTask(kReadWritePane1, "Wrote \"Record 03.\" at offset 20.");

    file.seekp(40, std::ios::beg);
    file << "Record 05.";
    dashboard.logTask(kReadWritePane1, "Wrote \"Record 05.\" at offset 40.");

    file.flush();
    const std::string contents = readWholeStream(file);
    dashboard.logTask(kReadWritePane1, "Combined file view: \"" + escapeForLog(contents) + '"');

    file.close();
    logSnapshots(dashboard, "Snapshot after Step 4");
}

void random_file_read_write_2(TaskDashboard& dashboard) {
    dashboard.logSystem("Step 5: Updating random_file_2.txt at new offsets.");
    ensureFileExists(kFile2);

    std::fstream file(kFile2, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        dashboard.logTask(kReadWritePane2, "Failed to open random_file_2.txt for read/write.");
        return;
    }

    file.seekp(10, std::ios::beg);
    file << "Record 02.";
    dashboard.logTask(kReadWritePane2, "Wrote \"Record 02.\" at offset 10.");

    file.seekp(30, std::ios::beg);
    file << "Record 04.";
    dashboard.logTask(kReadWritePane2, "Wrote \"Record 04.\" at offset 30.");

    file.seekp(60, std::ios::beg);
    file << "Record 07.";
    dashboard.logTask(kReadWritePane2, "Wrote \"Record 07.\" at offset 60.");

    file.flush();
    const std::string contents = readWholeStream(file);
    dashboard.logTask(kReadWritePane2, "Updated file view: \"" + escapeForLog(contents) + '"');

    file.close();
    logSnapshots(dashboard, "Snapshot after Step 5");
}
}

int main(int argc, char* argv[]) {
    TaskDashboard::DashboardLabels labels;
    labels.systemTitle = "Lab 14 Control";
    labels.sharedTitle = "File Snapshots";
    labels.taskTitles = {"Write File 1", "Read File 1", "Build File 2", "Update File 2"};

    TaskDashboard dashboard(4, determineDashboardMode(argc, argv), labels);
    dashboard.setHoldOnExit(determineHoldOnExit(argc, argv));

    dashboard.logSystem("Lab 14 Random Access I/O starting.");
    dashboard.logSystem("Working directory: " + std::filesystem::current_path().string());
    dashboard.logSystem("A real terminal uses ncurses automatically; non-TTY runs fall back to transcript mode.");
    logSnapshots(dashboard, "Initial file snapshot");

    random_file_write(dashboard);
    random_file_read(dashboard);
    random_file_read_write_1(dashboard);
    random_file_read_write_2(dashboard);

    dashboard.finish("Lab 14 run complete.");
    return 0;
}
