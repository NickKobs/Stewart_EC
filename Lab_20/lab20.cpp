#include "lab10_dashboard.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {
constexpr std::size_t kBlockSize = 512;
constexpr int kBlockCopies = 3;
constexpr int kCharWritePane = 0;
constexpr int kCharReadPane = 1;
constexpr int kBlockWritePane = 2;
constexpr int kBlockReadPane = 3;

const fs::path kDefaultCharDevice = "char_device.txt";
const fs::path kDefaultBlockDevice = "block_device.img";
constexpr std::string_view kDefaultCharInput = "This is a test of using a character device ... ... ...";
constexpr std::string_view kDefaultBlockInput = "This is a test of using a block device ..... ....... ........";

struct RunOptions {
    DashboardMode dashboardMode = DashboardMode::Auto;
    bool holdOnExit = true;
    bool showHelp = false;
    fs::path charDevicePath = kDefaultCharDevice;
    fs::path blockDevicePath = kDefaultBlockDevice;
    std::optional<std::string> charInput;
    std::optional<std::string> blockInput;
};

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

std::string trimBlockPayload(const std::array<char, kBlockSize>& block) {
    const auto end = std::find(block.begin(), block.end(), '\0');
    return std::string(block.begin(), end);
}

std::string describePathState(const fs::path& path) {
    std::error_code error;
    if (!fs::exists(path, error)) {
        return path.string() + " | missing";
    }

    const auto size = fs::file_size(path, error);
    if (error) {
        return path.string() + " | size unavailable: " + error.message();
    }

    return path.string() + " | size=" + std::to_string(size) + " bytes";
}

std::string makeCharPayload(std::string input) {
    const std::size_t quitOffset = input.find('q');
    if (quitOffset != std::string::npos) {
        input.resize(quitOffset);
    }
    return input;
}

std::string makeBlockPayload(std::string input) {
    if (input.size() >= kBlockSize) {
        input.resize(kBlockSize - 1);
    }
    return input;
}

bool hasInteractiveInput() {
    return isatty(STDIN_FILENO) != 0;
}

RunOptions parseArgs(int argc, char* argv[]) {
    RunOptions options;

    if (const char* envMode = std::getenv("LAB20_UI")) {
        const std::string_view mode(envMode);
        if (mode == "ncurses") {
            options.dashboardMode = DashboardMode::Ncurses;
        } else if (mode == "text") {
            options.dashboardMode = DashboardMode::Text;
        }
    }

    if (const char* envHold = std::getenv("LAB20_HOLD")) {
        const std::string_view hold(envHold);
        if (hold == "0" || hold == "false" || hold == "FALSE" || hold == "no") {
            options.holdOnExit = false;
        } else if (hold == "1" || hold == "true" || hold == "TRUE" || hold == "yes") {
            options.holdOnExit = true;
        }
    }

    if (const char* envChar = std::getenv("LAB20_CHAR_INPUT")) {
        options.charInput = envChar;
    }

    if (const char* envBlock = std::getenv("LAB20_BLOCK_INPUT")) {
        options.blockInput = envBlock;
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

        if (arg == "--char-input" && i + 1 < argc) {
            options.charInput = argv[++i];
            continue;
        }

        if (arg == "--block-input" && i + 1 < argc) {
            options.blockInput = argv[++i];
            continue;
        }

        if (arg == "--char-path" && i + 1 < argc) {
            options.charDevicePath = argv[++i];
            continue;
        }

        if (arg == "--block-path" && i + 1 < argc) {
            options.blockDevicePath = argv[++i];
            continue;
        }

        throw std::runtime_error("unknown or incomplete option: " + std::string(arg));
    }

    return options;
}

std::string resolveCharInput(TaskDashboard& dashboard, const RunOptions& options) {
    if (options.charInput.has_value()) {
        dashboard.logSystem("Using the provided character-device payload.");
        return makeCharPayload(*options.charInput);
    }

    if (hasInteractiveInput()) {
        dashboard.logSystem("Prompting for character-device input inside the active UI.");
        dashboard.logTask(kCharWritePane, "Enter characters; the first q ends the simulated stream.");
        return makeCharPayload(dashboard.promptInput("Enter characters (q to quit): ", 255));
    }

    dashboard.logSystem("No interactive stdin detected. Reusing the built-in character-device sample.");
    return std::string(kDefaultCharInput);
}

std::string resolveBlockInput(TaskDashboard& dashboard, const RunOptions& options) {
    if (options.blockInput.has_value()) {
        dashboard.logSystem("Using the provided block-device payload.");
        return makeBlockPayload(*options.blockInput);
    }

    if (hasInteractiveInput()) {
        dashboard.logSystem("Prompting for block-device input inside the active UI.");
        return makeBlockPayload(dashboard.promptInput("Enter text for block device: ", 255));
    }

    dashboard.logSystem("No interactive stdin detected. Reusing the built-in block-device sample.");
    return std::string(kDefaultBlockInput);
}

bool writeCharacterDevice(TaskDashboard& dashboard, const fs::path& path, const std::string& payload) {
    std::ofstream device(path, std::ios::binary | std::ios::trunc);
    if (!device.is_open()) {
        dashboard.logTask(kCharWritePane, "Failed to open " + path.string() + " for character-device output.");
        return false;
    }

    dashboard.logTask(kCharWritePane, "Writing one byte at a time to " + path.string());
    dashboard.logTask(kCharWritePane, "Captured payload: \"" + escapeForLog(payload) + '"');
    for (const char ch : payload) {
        device.put(ch);
    }

    device.flush();
    if (!device) {
        dashboard.logTask(kCharWritePane, "Write failed: " + std::string(std::strerror(errno)));
        return false;
    }

    device.close();
    if (!device) {
        dashboard.logTask(kCharWritePane, "Close failed after character-device write.");
        return false;
    }

    dashboard.logTask(
        kCharWritePane,
        "Data written to character device --> " + path.string() + " (" + std::to_string(payload.size()) + " bytes)"
    );
    dashboard.logQueue("CHAR write | " + describePathState(path));
    return true;
}

bool readCharacterDevice(TaskDashboard& dashboard, const fs::path& path) {
    std::ifstream device(path, std::ios::binary);
    if (!device.is_open()) {
        dashboard.logTask(kCharReadPane, "Failed to open " + path.string() + " for character-device input.");
        return false;
    }

    std::string payload;
    char ch = '\0';
    while (device.get(ch)) {
        payload.push_back(ch);
    }

    dashboard.logTask(kCharReadPane, "------------------------------------------------------------");
    dashboard.logTask(kCharReadPane, payload.empty() ? "(character device is empty)" : payload);
    dashboard.logTask(kCharReadPane, "------------------------------------------------------------");
    dashboard.logQueue("CHAR read  | " + describePathState(path));
    return true;
}

bool writeBlockDevice(TaskDashboard& dashboard, const fs::path& path, const std::string& payload) {
    std::ofstream device(path, std::ios::binary | std::ios::trunc);
    if (!device.is_open()) {
        dashboard.logTask(kBlockWritePane, "Failed to open " + path.string() + " for block-device output.");
        return false;
    }

    std::array<char, kBlockSize> block {};
    std::copy(payload.begin(), payload.end(), block.begin());

    dashboard.logTask(kBlockWritePane, "Preparing a zero-filled 512-byte block.");
    dashboard.logTask(kBlockWritePane, "Captured payload: \"" + escapeForLog(payload) + '"');
    for (int copy = 0; copy < kBlockCopies; ++copy) {
        device.write(block.data(), static_cast<std::streamsize>(block.size()));
        if (!device) {
            dashboard.logTask(kBlockWritePane, "Block write failed on copy " + std::to_string(copy + 1) + '.');
            return false;
        }

        dashboard.logTask(kBlockWritePane, "Wrote block copy " + std::to_string(copy + 1) + " of 3.");
    }

    device.flush();
    if (!device) {
        dashboard.logTask(kBlockWritePane, "Flush failed after writing the block-device image.");
        return false;
    }

    device.close();
    if (!device) {
        dashboard.logTask(kBlockWritePane, "Close failed after block-device write.");
        return false;
    }

    dashboard.logTask(
        kBlockWritePane,
        "Data written to block device --> " + path.string() + " (" +
            std::to_string(kBlockCopies * static_cast<int>(kBlockSize)) + " bytes total)"
    );
    dashboard.logQueue("BLOCK write | " + describePathState(path));
    return true;
}

bool readBlockDevice(TaskDashboard& dashboard, const fs::path& path) {
    std::ifstream device(path, std::ios::binary);
    if (!device.is_open()) {
        dashboard.logTask(kBlockReadPane, "Failed to open " + path.string() + " for block-device input.");
        return false;
    }

    std::array<char, kBlockSize> firstBlock {};
    std::array<char, kBlockSize> skippedBlock {};
    std::array<char, kBlockSize> thirdBlock {};

    device.read(firstBlock.data(), static_cast<std::streamsize>(firstBlock.size()));
    if (device.gcount() != static_cast<std::streamsize>(firstBlock.size())) {
        dashboard.logTask(kBlockReadPane, "Could not read the first 512-byte block.");
        return false;
    }

    device.read(skippedBlock.data(), static_cast<std::streamsize>(skippedBlock.size()));
    if (device.gcount() != static_cast<std::streamsize>(skippedBlock.size())) {
        dashboard.logTask(kBlockReadPane, "Could not skip the middle 512-byte block.");
        return false;
    }

    device.read(thirdBlock.data(), static_cast<std::streamsize>(thirdBlock.size()));
    if (device.gcount() != static_cast<std::streamsize>(thirdBlock.size())) {
        dashboard.logTask(kBlockReadPane, "Could not read the third 512-byte block.");
        return false;
    }

    dashboard.logTask(kBlockReadPane, "------------------------------------------------------------");
    dashboard.logTask(kBlockReadPane, trimBlockPayload(firstBlock));
    dashboard.logTask(kBlockReadPane, "");
    dashboard.logTask(kBlockReadPane, trimBlockPayload(thirdBlock));
    dashboard.logTask(kBlockReadPane, "------------------------------------------------------------");
    dashboard.logTask(kBlockReadPane, "The second block was intentionally read and skipped.");
    dashboard.logQueue("BLOCK read  | " + describePathState(path));
    return true;
}

void printUsage(const char* programName) {
    std::printf(
        "Usage: %s [--ncurses|--text] [--hold|--no-hold] [--char-path FILE] [--block-path FILE]\n",
        programName
    );
    std::printf("          [--char-input TEXT] [--block-input TEXT]\n");
    std::printf("Without explicit input, the program prompts when stdin is interactive.\n");
    std::printf("Otherwise it uses built-in sample payloads so transcript runs stay deterministic.\n");
}
}

int main(int argc, char* argv[]) {
    try {
        const RunOptions options = parseArgs(argc, argv);
        if (options.showHelp) {
            printUsage(argv[0]);
            return 0;
        }

        TaskDashboard::DashboardLabels labels;
        labels.systemTitle = "Lab 20 Device Drivers";
        labels.sharedTitle = "Device State";
        labels.taskTitles = {
            "Character Write",
            "Character Read",
            "Block Write",
            "Block Read",
        };

        TaskDashboard dashboard(4, options.dashboardMode, labels);
        dashboard.setHoldOnExit(options.holdOnExit);

        dashboard.logSystem("Simulating user-mode character and block device drivers.");
        dashboard.logSystem("Character devices write one byte at a time.");
        dashboard.logSystem("Block devices write fixed-size 512-byte buffers.");
        dashboard.logSystem("Transcript mode remains available when ncurses cannot attach to a real TTY.");
        dashboard.logQueue("CHAR path  | " + options.charDevicePath.string());
        dashboard.logQueue("BLOCK path | " + options.blockDevicePath.string());

        const std::string charPayload = resolveCharInput(dashboard, options);
        const std::string blockPayload = resolveBlockInput(dashboard, options);

        const bool charWriteOk = writeCharacterDevice(dashboard, options.charDevicePath, charPayload);
        const bool charReadOk = charWriteOk && readCharacterDevice(dashboard, options.charDevicePath);
        const bool blockWriteOk = writeBlockDevice(dashboard, options.blockDevicePath, blockPayload);
        const bool blockReadOk = blockWriteOk && readBlockDevice(dashboard, options.blockDevicePath);

        if (charWriteOk && charReadOk && blockWriteOk && blockReadOk) {
            dashboard.finish("Lab 20 simulation complete.");
            return 0;
        }

        dashboard.finish("Lab 20 simulation finished with one or more device errors.");
        return 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
