#include "lab10_dashboard.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
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
constexpr std::string_view kDefaultCharPayload = "This is a test of using a character device ... ... ...";
constexpr std::string_view kDefaultBlockPayload = "This is a test of using a block device ..... ....... ........";
constexpr std::string_view kSeparator = "------------------------------------------------------------";

struct RunOptions {
    DashboardMode dashboardMode = DashboardMode::Auto;
    bool holdOnExit = true;
    bool showHelp = false;
    fs::path charDevicePath = kDefaultCharDevice;
    fs::path blockDevicePath = kDefaultBlockDevice;
    std::optional<std::string> charInput;
    std::optional<std::string> blockInput;
};

struct InputCapture {
    std::string raw;
    std::string effective;
};

struct BlockReadback {
    std::string firstBlock;
    std::string thirdBlock;
};

bool stdoutAndStdinAreTtys() {
    return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
}

std::string escapeForLog(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (const unsigned char ch : text) {
        switch (ch) {
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\0':
                escaped += "\\0";
                break;
            default:
                escaped.push_back(static_cast<char>(ch));
                break;
        }
    }

    return escaped;
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

std::string clipCharPayload(const std::string& raw) {
    const std::size_t quitOffset = raw.find('q');
    if (quitOffset == std::string::npos) {
        return raw;
    }

    return raw.substr(0, quitOffset);
}

std::string clipBlockPayload(const std::string& raw) {
    if (raw.size() >= kBlockSize) {
        return raw.substr(0, kBlockSize - 1);
    }

    return raw;
}

std::string readTranscriptLine() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return {};
    }

    return line;
}

InputCapture resolveCharInputForTranscript(const RunOptions& options) {
    if (options.charInput.has_value()) {
        return {*options.charInput, clipCharPayload(*options.charInput)};
    }

    if (isatty(STDIN_FILENO) != 0) {
        const std::string raw = readTranscriptLine();
        return {raw, clipCharPayload(raw)};
    }

    return {std::string(kDefaultCharPayload) + 'q', std::string(kDefaultCharPayload)};
}

InputCapture resolveBlockInputForTranscript(const RunOptions& options) {
    if (options.blockInput.has_value()) {
        return {*options.blockInput, clipBlockPayload(*options.blockInput)};
    }

    if (isatty(STDIN_FILENO) != 0) {
        const std::string raw = readTranscriptLine();
        return {raw, clipBlockPayload(raw)};
    }

    return {std::string(kDefaultBlockPayload), std::string(kDefaultBlockPayload)};
}

InputCapture resolveCharInputForNcurses(TaskDashboard& dashboard, const RunOptions& options) {
    if (options.charInput.has_value()) {
        dashboard.logSystem("Using the provided character-device payload.");
        return {*options.charInput, clipCharPayload(*options.charInput)};
    }

    dashboard.logSystem("Prompting for character-device input inside the active UI.");
    dashboard.logTask(kCharWritePane, "Enter characters; the first q ends the simulated stream.");
    const std::string raw = dashboard.promptInput("Enter characters (q to quit): ", 255);
    return {raw, clipCharPayload(raw)};
}

InputCapture resolveBlockInputForNcurses(TaskDashboard& dashboard, const RunOptions& options) {
    if (options.blockInput.has_value()) {
        dashboard.logSystem("Using the provided block-device payload.");
        return {*options.blockInput, clipBlockPayload(*options.blockInput)};
    }

    dashboard.logSystem("Prompting for block-device input inside the active UI.");
    const std::string raw = dashboard.promptInput("Enter text for block device: ", 255);
    return {raw, clipBlockPayload(raw)};
}

bool writeCharDeviceFile(const fs::path& path, const std::string& payload, std::string& error) {
    std::ofstream device(path, std::ios::binary | std::ios::trunc);
    if (!device.is_open()) {
        error = "failed to open " + path.string() + " for character-device output";
        return false;
    }

    for (const char ch : payload) {
        device.put(ch);
    }

    device.flush();
    if (!device) {
        error = "character-device write failed";
        return false;
    }

    device.close();
    if (!device) {
        error = "character-device close failed";
        return false;
    }

    return true;
}

bool readCharDeviceFile(const fs::path& path, std::string& payload, std::string& error) {
    std::ifstream device(path, std::ios::binary);
    if (!device.is_open()) {
        error = "failed to open " + path.string() + " for character-device input";
        return false;
    }

    payload.clear();
    char ch = '\0';
    while (device.get(ch)) {
        payload.push_back(ch);
    }

    return true;
}

bool writeBlockDeviceFile(const fs::path& path, const std::string& payload, std::string& error) {
    std::ofstream device(path, std::ios::binary | std::ios::trunc);
    if (!device.is_open()) {
        error = "failed to open " + path.string() + " for block-device output";
        return false;
    }

    std::array<char, kBlockSize> block {};
    std::copy(payload.begin(), payload.end(), block.begin());

    for (int copy = 0; copy < kBlockCopies; ++copy) {
        device.write(block.data(), static_cast<std::streamsize>(block.size()));
        if (!device) {
            error = "block-device write failed";
            return false;
        }
    }

    device.flush();
    if (!device) {
        error = "block-device flush failed";
        return false;
    }

    device.close();
    if (!device) {
        error = "block-device close failed";
        return false;
    }

    return true;
}

bool readBlockDeviceFile(const fs::path& path, BlockReadback& readback, std::string& error) {
    std::ifstream device(path, std::ios::binary);
    if (!device.is_open()) {
        error = "failed to open " + path.string() + " for block-device input";
        return false;
    }

    auto readBlock = [&](std::array<char, kBlockSize>& block, const char* label) {
        device.read(block.data(), static_cast<std::streamsize>(block.size()));
        if (device.gcount() != static_cast<std::streamsize>(block.size())) {
            error = std::string("could not read ") + label;
            return false;
        }
        return true;
    };

    std::array<char, kBlockSize> first {};
    std::array<char, kBlockSize> skipped {};
    std::array<char, kBlockSize> third {};

    if (!readBlock(first, "the first 512-byte block")) {
        return false;
    }
    if (!readBlock(skipped, "the second 512-byte block")) {
        return false;
    }
    if (!readBlock(third, "the third 512-byte block")) {
        return false;
    }

    const auto trimBlock = [](const std::array<char, kBlockSize>& block) {
        const auto end = std::find(block.begin(), block.end(), '\0');
        return std::string(block.begin(), end);
    };

    readback.firstBlock = trimBlock(first);
    readback.thirdBlock = trimBlock(third);
    return true;
}

bool writeCharacterDevice(TaskDashboard& dashboard, const fs::path& path, const InputCapture& input) {
    std::string error;
    dashboard.logTask(kCharWritePane, "Writing one byte at a time to " + path.string());
    dashboard.logTask(kCharWritePane, "Captured payload: \"" + escapeForLog(input.effective) + '"');

    if (!writeCharDeviceFile(path, input.effective, error)) {
        dashboard.logTask(kCharWritePane, error);
        return false;
    }

    dashboard.logTask(
        kCharWritePane,
        "Data written to character device --> " + path.string() + " (" +
            std::to_string(input.effective.size()) + " bytes)"
    );
    dashboard.logQueue("CHAR write | " + describePathState(path));
    return true;
}

bool readCharacterDevice(TaskDashboard& dashboard, const fs::path& path) {
    std::string payload;
    std::string error;
    if (!readCharDeviceFile(path, payload, error)) {
        dashboard.logTask(kCharReadPane, error);
        return false;
    }

    dashboard.logTask(kCharReadPane, std::string(kSeparator));
    dashboard.logTask(kCharReadPane, payload.empty() ? "(character device is empty)" : payload);
    dashboard.logTask(kCharReadPane, std::string(kSeparator));
    dashboard.logQueue("CHAR read  | " + describePathState(path));
    return true;
}

bool writeBlockDevice(TaskDashboard& dashboard, const fs::path& path, const InputCapture& input) {
    std::string error;
    dashboard.logTask(kBlockWritePane, "Preparing a zero-filled 512-byte block.");
    dashboard.logTask(kBlockWritePane, "Captured payload: \"" + escapeForLog(input.effective) + '"');

    if (!writeBlockDeviceFile(path, input.effective, error)) {
        dashboard.logTask(kBlockWritePane, error);
        return false;
    }

    for (int copy = 0; copy < kBlockCopies; ++copy) {
        dashboard.logTask(kBlockWritePane, "Wrote block copy " + std::to_string(copy + 1) + " of 3.");
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
    BlockReadback readback;
    std::string error;
    if (!readBlockDeviceFile(path, readback, error)) {
        dashboard.logTask(kBlockReadPane, error);
        return false;
    }

    dashboard.logTask(kBlockReadPane, std::string(kSeparator));
    dashboard.logTask(kBlockReadPane, readback.firstBlock);
    dashboard.logTask(kBlockReadPane, "");
    dashboard.logTask(kBlockReadPane, readback.thirdBlock);
    dashboard.logTask(kBlockReadPane, std::string(kSeparator));
    dashboard.logTask(kBlockReadPane, "The second block was intentionally read and skipped.");
    dashboard.logQueue("BLOCK read  | " + describePathState(path));
    return true;
}

int runNcursesSimulation(const RunOptions& options) {
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
    dashboard.logQueue("CHAR path  | " + options.charDevicePath.string());
    dashboard.logQueue("BLOCK path | " + options.blockDevicePath.string());

    const InputCapture charInput = resolveCharInputForNcurses(dashboard, options);
    const InputCapture blockInput = resolveBlockInputForNcurses(dashboard, options);

    const bool charWriteOk = writeCharacterDevice(dashboard, options.charDevicePath, charInput);
    const bool charReadOk = charWriteOk && readCharacterDevice(dashboard, options.charDevicePath);
    const bool blockWriteOk = writeBlockDevice(dashboard, options.blockDevicePath, blockInput);
    const bool blockReadOk = blockWriteOk && readBlockDevice(dashboard, options.blockDevicePath);

    if (charWriteOk && charReadOk && blockWriteOk && blockReadOk) {
        dashboard.finish("Lab 20 simulation complete.");
        return 0;
    }

    dashboard.finish("Lab 20 simulation finished with one or more device errors.");
    return 1;
}

int runTranscriptSimulation(const RunOptions& options) {
    const InputCapture charInput = resolveCharInputForTranscript(options);
    const InputCapture blockInput = resolveBlockInputForTranscript(options);

    std::string error;
    std::string charOutput;
    BlockReadback blockOutput;

    std::printf("Write to a CHAR device:\n");
    std::printf("Enter characters (q to quit):\n");
    std::printf("%s\n", charInput.raw.c_str());

    if (!writeCharDeviceFile(options.charDevicePath, charInput.effective, error)) {
        std::fprintf(stderr, "Error: %s\n", error.c_str());
        return 1;
    }

    std::printf("Data written to character device --> %s\n\n", options.charDevicePath.string().c_str());

    if (!readCharDeviceFile(options.charDevicePath, charOutput, error)) {
        std::fprintf(stderr, "Error: %s\n", error.c_str());
        return 1;
    }

    std::printf("Read and display the content of CHAR device:\n");
    std::printf("%s\n", std::string(kSeparator).c_str());
    std::printf("%s\n", charOutput.c_str());
    std::printf("%s\n\n", std::string(kSeparator).c_str());

    std::printf("Write to a BLOCK device:\n");
    std::printf("Enter text for block device: (hit ENTER key to quit)\n");
    std::printf("%s\n", blockInput.raw.c_str());

    if (!writeBlockDeviceFile(options.blockDevicePath, blockInput.effective, error)) {
        std::fprintf(stderr, "Error: %s\n", error.c_str());
        return 1;
    }

    std::printf(
        "Data written to block device --> %s (512-byte block size).\n\n",
        options.blockDevicePath.string().c_str()
    );

    if (!readBlockDeviceFile(options.blockDevicePath, blockOutput, error)) {
        std::fprintf(stderr, "Error: %s\n", error.c_str());
        return 1;
    }

    std::printf("Read and display the content of BLOCK device:\n");
    std::printf("%s\n", std::string(kSeparator).c_str());
    std::printf("%s\n\n", blockOutput.firstBlock.c_str());
    std::printf("%s\n", blockOutput.thirdBlock.c_str());
    std::printf("%s\n", std::string(kSeparator).c_str());
    return 0;
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

void printUsage(const char* programName) {
    std::printf(
        "Usage: %s [--ncurses|--text] [--hold|--no-hold] [--char-path FILE] [--block-path FILE]\n",
        programName
    );
    std::printf("          [--char-input TEXT] [--block-input TEXT]\n");
    std::printf("ncurses mode provides the windowed dashboard; text mode mirrors the lab handout output.\n");
}
}

int main(int argc, char* argv[]) {
    try {
        const RunOptions options = parseArgs(argc, argv);
        if (options.showHelp) {
            printUsage(argv[0]);
            return 0;
        }

        const bool useNcurses = options.dashboardMode != DashboardMode::Text && stdoutAndStdinAreTtys();
        if (useNcurses) {
            return runNcursesSimulation(options);
        }

        return runTranscriptSimulation(options);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
