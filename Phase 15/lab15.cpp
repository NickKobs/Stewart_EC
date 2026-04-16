#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__linux__)
#include <sys/sysmacros.h>
#endif
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "lab10_dashboard.h"

namespace fs = std::filesystem;

enum class StatCallKind {
    LStat,
    Stat,
    FStat,
};

struct RunOptions {
    DashboardMode dashboardMode = DashboardMode::Auto;
    bool holdOnExit = true;
    bool showHelp = false;
    std::vector<fs::path> requestedPaths;
};

struct SampleDescriptor {
    fs::path path;
    std::string title;
};

struct PreparedRun {
    std::vector<SampleDescriptor> targets;
    std::vector<std::string> notes;
};

struct MetadataSnapshot {
    StatCallKind kind;
    fs::path path;
    struct stat data {};
};

struct SnapshotResult {
    std::optional<MetadataSnapshot> snapshot;
    std::string error;
};

RunOptions parseArgs(int argc, char* argv[]);
PreparedRun prepareRun(const RunOptions& options);
fs::path buildFixture(TaskDashboard& dashboard);
PreparedRun buildFixtureRun();
void writeFileIfMissing(const fs::path& path, std::string_view contents, mode_t mode, std::vector<std::string>& notes);
void createHardLinkIfMissing(const fs::path& target, const fs::path& linkPath, std::vector<std::string>& notes);
void createSymlinkIfMissing(const fs::path& target, const fs::path& linkPath, std::vector<std::string>& notes);

SnapshotResult collectSnapshot(StatCallKind kind, const fs::path& path);
std::string statCallName(StatCallKind kind);
std::string describeType(mode_t mode);
std::string formatPermissions(mode_t mode);
std::string formatTimestamp(std::time_t timestamp);
std::string formatDeviceId(dev_t deviceId);
std::vector<std::string> buildSnapshotLines(const MetadataSnapshot& snapshot);
std::string summarizeSnapshots(
    const std::string& title,
    const SnapshotResult& lstatResult,
    const SnapshotResult& statResult,
    const SnapshotResult& fstatResult
);
std::string fileTitleForPath(const fs::path& path);
void logPaneHeader(TaskDashboard& dashboard, int paneIndex, const fs::path& path);
void logSnapshot(TaskDashboard& dashboard, int paneIndex, const SnapshotResult& result);
void printUsage(const char* programName);

int main(int argc, char* argv[]) {
    const RunOptions options = parseArgs(argc, argv);
    if (options.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    const PreparedRun prepared = prepareRun(options);
    if (prepared.targets.empty()) {
        std::fprintf(stderr, "No files were selected for metadata inspection.\n");
        return 1;
    }

    TaskDashboard::DashboardLabels labels;
    labels.systemTitle = "Lab 15 Metadata";
    labels.sharedTitle = "Call Summary";
    for (const auto& target : prepared.targets) {
        labels.taskTitles.push_back(target.title);
    }

    TaskDashboard dashboard(static_cast<int>(prepared.targets.size()), options.dashboardMode, labels);
    dashboard.setHoldOnExit(options.holdOnExit);

    dashboard.logSystem("Comparing lstat(), stat(), and fstat() on each selected path.");
    dashboard.logSystem("lstat() inspects the path entry itself, stat() follows symlinks, and fstat() inspects an open descriptor.");
    for (const auto& note : prepared.notes) {
        dashboard.logSystem(note);
    }

    for (std::size_t i = 0; i < prepared.targets.size(); ++i) {
        const auto& target = prepared.targets[i];
        const int paneIndex = static_cast<int>(i);
        logPaneHeader(dashboard, paneIndex, target.path);

        const SnapshotResult lstatResult = collectSnapshot(StatCallKind::LStat, target.path);
        const SnapshotResult statResult = collectSnapshot(StatCallKind::Stat, target.path);
        const SnapshotResult fstatResult = collectSnapshot(StatCallKind::FStat, target.path);

        logSnapshot(dashboard, paneIndex, lstatResult);
        logSnapshot(dashboard, paneIndex, statResult);
        logSnapshot(dashboard, paneIndex, fstatResult);
        dashboard.logQueue(summarizeSnapshots(target.title, lstatResult, statResult, fstatResult));
    }

    dashboard.finish("Metadata extraction complete.");
    return 0;
}

RunOptions parseArgs(int argc, char* argv[]) {
    RunOptions options;

    const char* envMode = std::getenv("LAB15_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "ncurses") {
            options.dashboardMode = DashboardMode::Ncurses;
        } else if (mode == "text") {
            options.dashboardMode = DashboardMode::Text;
        }
    }

    const char* envHold = std::getenv("LAB15_HOLD");
    if (envHold != nullptr) {
        const std::string_view hold(envHold);
        if (hold == "0" || hold == "false" || hold == "FALSE" || hold == "no") {
            options.holdOnExit = false;
        } else if (hold == "1" || hold == "true" || hold == "TRUE" || hold == "yes") {
            options.holdOnExit = true;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--ncurses") {
            options.dashboardMode = DashboardMode::Ncurses;
            continue;
        }

        if (arg == "--text") {
            options.dashboardMode = DashboardMode::Text;
            continue;
        }

        if (arg == "--hold") {
            options.holdOnExit = true;
            continue;
        }

        if (arg == "--no-hold") {
            options.holdOnExit = false;
            continue;
        }

        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
            continue;
        }

        options.requestedPaths.emplace_back(argv[i]);
    }

    return options;
}

PreparedRun prepareRun(const RunOptions& options) {
    if (!options.requestedPaths.empty()) {
        PreparedRun prepared;
        prepared.notes.push_back("Using command-line paths. Fixture generation skipped.");
        for (const auto& path : options.requestedPaths) {
            prepared.targets.push_back({path, fileTitleForPath(path)});
        }
        return prepared;
    }

    return buildFixtureRun();
}

PreparedRun buildFixtureRun() {
    PreparedRun prepared;
    std::error_code error;
    const fs::path fixtureRoot = fs::current_path(error) / "lab15_fixture";
    if (error) {
        prepared.notes.push_back("Failed to read current directory: " + error.message());
        return prepared;
    }

    fs::create_directories(fixtureRoot, error);
    if (error) {
        prepared.notes.push_back("Failed to create fixture directory: " + error.message());
        return prepared;
    }

    prepared.notes.push_back("Preparing lab fixture in " + fixtureRoot.string());

    writeFileIfMissing(
        fixtureRoot / "random_file_1.txt",
        "Lab 15 sample text file.\nThis file demonstrates metadata extraction.\n",
        0644,
        prepared.notes
    );
    writeFileIfMissing(
        fixtureRoot / "a.exe",
        "#!/bin/sh\necho 'Lab 15 sample executable'\n",
        0755,
        prepared.notes
    );
    writeFileIfMissing(
        fixtureRoot / "i-nodes",
        "Hard-link source for metadata inspection.\n",
        0644,
        prepared.notes
    );

    createHardLinkIfMissing(fixtureRoot / "i-nodes", fixtureRoot / "mylink", prepared.notes);
    createSymlinkIfMissing(fixtureRoot / "i-nodes", fixtureRoot / "mysymlink", prepared.notes);

    prepared.targets = {
        {fixtureRoot / "random_file_1.txt", "random_file_1.txt"},
        {fixtureRoot / "a.exe", "a.exe"},
        {fixtureRoot, "lab15_fixture"},
        {fixtureRoot / "i-nodes", "i-nodes"},
        {fixtureRoot / "mylink", "mylink"},
        {fixtureRoot / "mysymlink", "mysymlink"},
    };
    return prepared;
}

void writeFileIfMissing(
    const fs::path& path,
    const std::string_view contents,
    const mode_t mode,
    std::vector<std::string>& notes
) {
    const fs::file_status status = fs::symlink_status(path);
    if (status.type() != fs::file_type::not_found) {
        notes.push_back("Reusing existing fixture entry: " + path.string());
        return;
    }

    std::ofstream output(path);
    output << contents;
    output.close();
    if (!output) {
        notes.push_back("Failed to write fixture file: " + path.string());
        return;
    }

    if (::chmod(path.c_str(), mode) != 0) {
        notes.push_back(
            "Created fixture file but could not set permissions on " + path.string() + ": " + std::strerror(errno)
        );
        return;
    }

    notes.push_back("Created fixture file: " + path.string());
}

void createHardLinkIfMissing(const fs::path& target, const fs::path& linkPath, std::vector<std::string>& notes) {
    std::error_code error;
    const fs::file_status linkStatus = fs::symlink_status(linkPath, error);
    if (!error && linkStatus.type() != fs::file_type::not_found) {
        notes.push_back("Reusing existing hard-link entry: " + linkPath.string());
        return;
    }

    error.clear();
    fs::create_hard_link(target, linkPath, error);
    if (error) {
        notes.push_back("Failed to create hard link " + linkPath.string() + ": " + error.message());
        return;
    }

    notes.push_back("Created hard link: " + linkPath.string() + " -> " + target.filename().string());
}

void createSymlinkIfMissing(const fs::path& target, const fs::path& linkPath, std::vector<std::string>& notes) {
    std::error_code error;
    const fs::file_status linkStatus = fs::symlink_status(linkPath, error);
    if (!error && linkStatus.type() != fs::file_type::not_found) {
        notes.push_back("Reusing existing symlink entry: " + linkPath.string());
        return;
    }

    error.clear();
    fs::create_symlink(target.filename(), linkPath, error);
    if (error) {
        notes.push_back("Failed to create symlink " + linkPath.string() + ": " + error.message());
        return;
    }

    notes.push_back("Created symlink: " + linkPath.string() + " -> " + target.filename().string());
}

SnapshotResult collectSnapshot(const StatCallKind kind, const fs::path& path) {
    SnapshotResult result;
    struct stat snapshotData {};

    auto buildError = [&](const std::string& prefix) {
        result.error = prefix + ": " + std::strerror(errno);
    };

    if (kind == StatCallKind::LStat) {
        if (::lstat(path.c_str(), &snapshotData) != 0) {
            buildError("lstat failed");
            return result;
        }
    } else if (kind == StatCallKind::Stat) {
        if (::stat(path.c_str(), &snapshotData) != 0) {
            buildError("stat failed");
            return result;
        }
    } else {
        const int descriptor = ::open(path.c_str(), O_RDONLY);
        if (descriptor < 0) {
            buildError("open for fstat failed");
            return result;
        }

        if (::fstat(descriptor, &snapshotData) != 0) {
            buildError("fstat failed");
            ::close(descriptor);
            return result;
        }

        ::close(descriptor);
    }

    result.snapshot = MetadataSnapshot{kind, path, snapshotData};
    return result;
}

std::string statCallName(const StatCallKind kind) {
    switch (kind) {
        case StatCallKind::LStat:
            return "lstat()";
        case StatCallKind::Stat:
            return "stat()";
        case StatCallKind::FStat:
            return "fstat()";
    }

    return "unknown";
}

std::string describeType(const mode_t mode) {
    switch (mode & S_IFMT) {
        case S_IFBLK:
            return "block device";
        case S_IFCHR:
            return "character device";
        case S_IFDIR:
            return "directory";
        case S_IFIFO:
            return "FIFO/pipe";
        case S_IFLNK:
            return "symlink";
        case S_IFREG:
            return "regular file";
        case S_IFSOCK:
            return "socket";
        default:
            return "unknown";
    }
}

std::string formatPermissions(const mode_t mode) {
    std::string permissions(10, '-');
    switch (mode & S_IFMT) {
        case S_IFDIR:
            permissions[0] = 'd';
            break;
        case S_IFLNK:
            permissions[0] = 'l';
            break;
        case S_IFBLK:
            permissions[0] = 'b';
            break;
        case S_IFCHR:
            permissions[0] = 'c';
            break;
        case S_IFIFO:
            permissions[0] = 'p';
            break;
        case S_IFSOCK:
            permissions[0] = 's';
            break;
        default:
            permissions[0] = '-';
            break;
    }

    permissions[1] = (mode & S_IRUSR) ? 'r' : '-';
    permissions[2] = (mode & S_IWUSR) ? 'w' : '-';
    permissions[3] = (mode & S_IXUSR) ? 'x' : '-';
    permissions[4] = (mode & S_IRGRP) ? 'r' : '-';
    permissions[5] = (mode & S_IWGRP) ? 'w' : '-';
    permissions[6] = (mode & S_IXGRP) ? 'x' : '-';
    permissions[7] = (mode & S_IROTH) ? 'r' : '-';
    permissions[8] = (mode & S_IWOTH) ? 'w' : '-';
    permissions[9] = (mode & S_IXOTH) ? 'x' : '-';

    if (mode & S_ISUID) {
        permissions[3] = (mode & S_IXUSR) ? 's' : 'S';
    }
    if (mode & S_ISGID) {
        permissions[6] = (mode & S_IXGRP) ? 's' : 'S';
    }
    if (mode & S_ISVTX) {
        permissions[9] = (mode & S_IXOTH) ? 't' : 'T';
    }

    return permissions;
}

std::string formatTimestamp(const std::time_t timestamp) {
    std::tm localTime {};
    if (localtime_r(&timestamp, &localTime) == nullptr) {
        return "unavailable";
    }

    std::ostringstream builder;
    builder << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S %Z");
    return builder.str();
}

std::string formatDeviceId(const dev_t deviceId) {
    std::ostringstream builder;
    builder << "[" << static_cast<long long>(major(deviceId)) << "," << static_cast<long long>(minor(deviceId)) << "]";
    return builder.str();
}

std::vector<std::string> buildSnapshotLines(const MetadataSnapshot& snapshot) {
    std::vector<std::string> lines;
    const struct stat& st = snapshot.data;

    lines.push_back("== " + statCallName(snapshot.kind) + " ==");
    lines.push_back("Path: " + snapshot.path.string());
    lines.push_back("Device: " + formatDeviceId(st.st_dev));
    lines.push_back("File type: " + describeType(st.st_mode));
    lines.push_back("I-node: " + std::to_string(static_cast<long long>(st.st_ino)));

    std::ostringstream modeLine;
    modeLine << "Mode: 0" << std::oct << static_cast<unsigned long>(st.st_mode) << std::dec;
    lines.push_back(modeLine.str());
    lines.push_back("Permissions: " + formatPermissions(st.st_mode));

    if (st.st_mode & S_ISUID) {
        lines.push_back("Set-User-ID bit: set");
    }
    if (st.st_mode & S_ISGID) {
        lines.push_back("Set-Group-ID bit: set");
    }
    if (st.st_mode & S_ISVTX) {
        lines.push_back("Sticky bit: set");
    }

    lines.push_back("Link count: " + std::to_string(static_cast<long long>(st.st_nlink)));
    lines.push_back("Owner UID/GID: " + std::to_string(static_cast<long long>(st.st_uid)) + "/" +
                    std::to_string(static_cast<long long>(st.st_gid)));
    lines.push_back("Preferred block size: " + std::to_string(static_cast<long long>(st.st_blksize)) + " bytes");
    lines.push_back("File size: " + std::to_string(static_cast<long long>(st.st_size)) + " bytes");
    lines.push_back("Allocated blocks: " + std::to_string(static_cast<long long>(st.st_blocks)));
    lines.push_back("Status change: " + formatTimestamp(st.st_ctime));
    lines.push_back("Last access: " + formatTimestamp(st.st_atime));
    lines.push_back("Last modification: " + formatTimestamp(st.st_mtime));

    return lines;
}

std::string summarizeSnapshots(
    const std::string& title,
    const SnapshotResult& lstatResult,
    const SnapshotResult& statResult,
    const SnapshotResult& fstatResult
) {
    std::ostringstream summary;
    summary << title << " | ";

    const auto addSummary = [&](const char* label, const SnapshotResult& result) {
        summary << label << "=";
        if (!result.snapshot.has_value()) {
            summary << "ERROR(" << result.error << ") ";
            return;
        }

        const auto& st = result.snapshot->data;
        summary << describeType(st.st_mode) << ", inode=" << static_cast<long long>(st.st_ino) << ", size="
                << static_cast<long long>(st.st_size) << " ";
    };

    addSummary("lstat", lstatResult);
    addSummary("stat", statResult);
    addSummary("fstat", fstatResult);
    return summary.str();
}

std::string fileTitleForPath(const fs::path& path) {
    if (path.filename().empty()) {
        return path.string();
    }
    return path.filename().string();
}

void logPaneHeader(TaskDashboard& dashboard, const int paneIndex, const fs::path& path) {
    dashboard.logTask(paneIndex, "----------------------------------------");
    dashboard.logTask(paneIndex, "Inspecting " + path.string());
}

void logSnapshot(TaskDashboard& dashboard, const int paneIndex, const SnapshotResult& result) {
    if (!result.snapshot.has_value()) {
        dashboard.logTask(paneIndex, "== error ==");
        dashboard.logTask(paneIndex, result.error);
        return;
    }

    for (const auto& line : buildSnapshotLines(*result.snapshot)) {
        dashboard.logTask(paneIndex, line);
    }
}

void printUsage(const char* programName) {
    std::printf("Usage: %s [--ncurses|--text] [--hold|--no-hold] [paths...]\n", programName);
    std::printf("Without explicit paths, the program prepares a lab15_fixture directory in the current working directory.\n");
}
