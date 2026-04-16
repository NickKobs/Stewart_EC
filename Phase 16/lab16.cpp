#include <algorithm>
#include <array>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "lab10_dashboard.h"

namespace {
constexpr int kInodeCount = 8;

struct INode {
    std::array<char, 8> filenm{};
    std::int32_t t_id = -1;
    std::int32_t filesize = -1;
    std::uint8_t blocks = 0x00;
    std::array<std::uint8_t, 3> reserved{};
};

static_assert(std::is_trivially_copyable_v<INode>, "INode must stay binary-writable.");
static_assert(sizeof(INode) == 20, "INode layout must stay deterministic.");

DashboardMode determineDashboardMode(int argc, char* argv[]) {
    const char* envMode = std::getenv("LAB16_UI");
    if (envMode != nullptr) {
        const std::string_view mode(envMode);
        if (mode == "text") {
            return DashboardMode::Text;
        }
        if (mode == "auto") {
            return DashboardMode::Auto;
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
        if (arg == "--auto") {
            return DashboardMode::Auto;
        }
        if (arg == "--ncurses") {
            return DashboardMode::Ncurses;
        }
    }

    return DashboardMode::Ncurses;
}

std::string inodeName(const INode& inode) {
    const auto end = std::find(inode.filenm.begin(), inode.filenm.end(), '\0');
    return std::string(inode.filenm.begin(), end);
}

INode makeInode(const std::string_view name, const std::int32_t taskId, const std::int32_t fileSize, const std::uint8_t blocks) {
    INode inode{};
    const std::size_t copyLength = std::min(name.size(), inode.filenm.size() - 1);
    std::copy_n(name.begin(), copyLength, inode.filenm.begin());
    inode.t_id = taskId;
    inode.filesize = fileSize;
    inode.blocks = blocks;
    return inode;
}

std::string blocksToBinary(const std::uint8_t value) {
    return std::bitset<8>(value).to_string();
}

std::streamoff offsetForIndex(const int index) {
    return static_cast<std::streamoff>(index) * static_cast<std::streamoff>(sizeof(INode));
}

void initializeInodeFile(const std::filesystem::path& filePath, const int inodeCount, const INode& defaultNode) {
    std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to create inode file: " + filePath.string());
    }

    for (int i = 0; i < inodeCount; ++i) {
        output.write(reinterpret_cast<const char*>(&defaultNode), static_cast<std::streamsize>(sizeof(defaultNode)));
    }
}

void writeInode(const std::filesystem::path& filePath, const std::streamoff offset, const INode& inode) {
    std::fstream file(filePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        throw std::runtime_error("failed to open inode file for writing: " + filePath.string());
    }

    file.seekp(offset, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&inode), static_cast<std::streamsize>(sizeof(inode)));
}

INode readInode(const std::filesystem::path& filePath, const std::streamoff offset) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open inode file for reading: " + filePath.string());
    }

    INode inode{};
    input.seekg(offset, std::ios::beg);
    input.read(reinterpret_cast<char*>(&inode), static_cast<std::streamsize>(sizeof(inode)));
    if (!input) {
        throw std::runtime_error("failed to read inode at byte offset " + std::to_string(offset));
    }

    return inode;
}

std::string formatTableRow(const int index, const INode& inode) {
    std::ostringstream row;
    row << std::left
        << std::setw(4) << index
        << std::setw(8) << inode.t_id
        << std::setw(12) << inodeName(inode)
        << std::setw(10) << inode.filesize
        << blocksToBinary(inode.blocks);
    return row.str();
}

std::string formatReadResult(const int index, const INode& inode) {
    std::ostringstream row;
    row << "inode[" << index << "]"
        << " tid=" << inode.t_id
        << " name=" << inodeName(inode)
        << " size=" << inode.filesize
        << " blocks=" << blocksToBinary(inode.blocks);
    return row.str();
}

std::vector<std::string> buildTableSnapshot(const std::filesystem::path& filePath, const std::string& title) {
    std::vector<std::string> lines;
    lines.push_back(title);
    lines.push_back("slot t_id  filename    size      blocks");
    for (int index = 0; index < kInodeCount; ++index) {
        lines.push_back(formatTableRow(index, readInode(filePath, offsetForIndex(index))));
    }
    return lines;
}

std::vector<std::string> buildRawSnapshot(const std::filesystem::path& filePath, const std::string& title) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open inode file for raw dump: " + filePath.string());
    }

    std::vector<std::string> lines;
    lines.push_back(title);

    for (int index = 0; index < kInodeCount; ++index) {
        std::array<unsigned char, sizeof(INode)> bytes{};
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("failed while reading raw bytes for inode " + std::to_string(index));
        }

        std::ostringstream row;
        row << "slot " << index << " | ";
        row << std::hex << std::uppercase << std::setfill('0');
        for (const unsigned char byte : bytes) {
            row << std::setw(2) << static_cast<int>(byte) << ' ';
        }
        row << "| ";
        for (const unsigned char byte : bytes) {
            row << (std::isprint(static_cast<int>(byte)) ? static_cast<char>(byte) : '.');
        }
        lines.push_back(row.str());
    }

    return lines;
}

void logLines(TaskDashboard& dashboard, const int taskId, const std::vector<std::string>& lines) {
    for (const auto& line : lines) {
        dashboard.logTask(taskId, line);
    }
}

void logTable(TaskDashboard& dashboard, const std::filesystem::path& filePath, const std::string& title) {
    dashboard.logQueue("");
    for (const auto& line : buildTableSnapshot(filePath, title)) {
        dashboard.logQueue(line);
    }
}

void runLab16(TaskDashboard& dashboard) {
    const std::filesystem::path filePath = std::filesystem::current_path() / "i-nodes.bin";
    const INode defaultNode = makeInode("empty", -1, -1, 0x00);

    dashboard.logSystem("Lab 16 inode dashboard online.");
    dashboard.logSystem("Binary inode file: " + filePath.string());
    dashboard.logTask(0, "Step 1: initialize 8 inode slots with the default empty record.");

    initializeInodeFile(filePath, kInodeCount, defaultNode);
    logTable(dashboard, filePath, "Snapshot after initialization");
    logLines(dashboard, 1, buildRawSnapshot(filePath, "Raw bytes after initialization"));

    const INode file1 = makeInode("file1", 1, 50, 0x80);
    dashboard.logTask(0, "Step 2: write file1 to slot 0.");
    writeInode(filePath, offsetForIndex(0), file1);

    const INode file2 = makeInode("file2", 2, 150, 0x60);
    dashboard.logTask(0, "Step 3: write file2 to slot 3.");
    writeInode(filePath, offsetForIndex(3), file2);

    logTable(dashboard, filePath, "Snapshot after writes");
    logLines(dashboard, 1, buildRawSnapshot(filePath, "Raw bytes after writes"));

    dashboard.logTask(2, "Step 4: read slot 2 (expected empty).");
    dashboard.logTask(2, formatReadResult(2, readInode(filePath, offsetForIndex(2))));
    dashboard.logTask(2, "Step 5: read slot 0 (expected file1).");
    dashboard.logTask(2, formatReadResult(0, readInode(filePath, offsetForIndex(0))));
    dashboard.logTask(2, "Step 6: read slot 3 (expected file2).");
    dashboard.logTask(2, formatReadResult(3, readInode(filePath, offsetForIndex(3))));

    logTable(dashboard, filePath, "Final inode table");
    dashboard.finish("Lab 16 run complete.");
}
}

int main(int argc, char* argv[]) {
    try {
        TaskDashboard::DashboardLabels labels;
        labels.systemTitle = "Lab 16 Inodes";
        labels.sharedTitle = "Inode Table";
        labels.taskTitles = {"Operations", "Raw Bytes", "Read Results"};

        TaskDashboard dashboard(3, determineDashboardMode(argc, argv), labels);
        runLab16(dashboard);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "lab16 error: " << error.what() << '\n';
        return 1;
    }
}
