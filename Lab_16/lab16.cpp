#include <algorithm>
#include <array>
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

struct i_node {
    std::array<char, 8> filenm{};
    std::int32_t t_id = -1;
    std::int32_t filesize = -1;
    std::uint8_t blocks = 0x00;
    std::array<std::uint8_t, 3> reserved{};
};

static_assert(std::is_trivially_copyable_v<i_node>, "i_node must stay binary-writable.");
static_assert(sizeof(i_node) == 20, "i_node layout must stay deterministic.");

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

std::string inode_name(const i_node& inode) {
    const auto end = std::find(inode.filenm.begin(), inode.filenm.end(), '\0');
    return std::string(inode.filenm.begin(), end);
}

i_node make_i_node(const std::string_view name, const std::int32_t taskId, const std::int32_t fileSize, const std::uint8_t blocks) {
    i_node inode{};
    const std::size_t copyLength = std::min(name.size(), inode.filenm.size() - 1);
    std::copy_n(name.begin(), copyLength, inode.filenm.begin());
    inode.t_id = taskId;
    inode.filesize = fileSize;
    inode.blocks = blocks;
    return inode;
}

std::string char_to_binary(const unsigned char value) {
    char result[sizeof(value) * 8 + 1];
    unsigned char mask = 0x80;
    int i = 0;

    for (; i < static_cast<int>(sizeof(value) * 8); ++i) {
        result[i] = ((value & mask) == 0U) ? '0' : '1';
        mask >>= 1;
    }

    result[i] = '\0';
    return std::string(result);
}

int inode_offset(const int i_node_index) {
    return i_node_index * static_cast<int>(sizeof(i_node));
}

void init_inode(
    const std::filesystem::path& file_name,
    const int num_i_nodes,
    const int i_node_size,
    const i_node& default_i_node
) {
    std::ofstream output(file_name, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to create inode file: " + file_name.string());
    }

    for (int i = 0; i < num_i_nodes; ++i) {
        output.write(reinterpret_cast<const char*>(&default_i_node), static_cast<std::streamsize>(i_node_size));
    }

    if (!output) {
        throw std::runtime_error("failed while initializing inode file: " + file_name.string());
    }
}

void write_inode(
    const std::filesystem::path& file_name,
    const int offset,
    const i_node& the_i_node,
    const int i_node_size
) {
    std::fstream file(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        throw std::runtime_error("failed to open inode file for writing: " + file_name.string());
    }

    file.seekp(offset, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&the_i_node), static_cast<std::streamsize>(i_node_size));
    if (!file) {
        throw std::runtime_error("failed to write inode at byte offset " + std::to_string(offset));
    }
}

void read_inode(
    const std::filesystem::path& file_name,
    const int offset,
    i_node& the_i_node,
    const int i_node_size
) {
    std::ifstream input(file_name, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open inode file for reading: " + file_name.string());
    }

    input.seekg(offset, std::ios::beg);
    input.read(reinterpret_cast<char*>(&the_i_node), static_cast<std::streamsize>(i_node_size));
    if (!input) {
        throw std::runtime_error("failed to read inode at byte offset " + std::to_string(offset));
    }
}

std::vector<std::string> dump_inode(const std::filesystem::path& file_name, const int i_node_size) {
    std::ifstream input(file_name, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open inode file for raw dump: " + file_name.string());
    }

    std::vector<std::string> lines;
    lines.push_back("Raw serialized i-node records");

    for (int index = 0; index < kInodeCount; ++index) {
        std::vector<unsigned char> bytes(static_cast<std::size_t>(i_node_size));
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

std::vector<std::string> dump_inode(const std::filesystem::path& file_name) {
    std::vector<std::string> lines;
    lines.push_back("==============================================");

    std::ostringstream header;
    header << std::left
           << std::setw(8) << "T-ID"
           << std::setw(12) << "FileName"
           << std::setw(12) << "FileSize"
           << "Blocks";
    lines.push_back(header.str());

    for (int index = 0; index < kInodeCount; ++index) {
        i_node inode{};
        read_inode(file_name, inode_offset(index), inode, static_cast<int>(sizeof(i_node)));

        std::ostringstream row;
        row << std::left
            << std::setw(8) << inode.t_id
            << std::setw(12) << inode_name(inode)
            << std::setw(12) << inode.filesize
            << char_to_binary(inode.blocks);
        lines.push_back(row.str());
    }

    lines.push_back("==============================================");
    return lines;
}

std::string format_read_result(const int index, const i_node& inode) {
    std::ostringstream row;
    row << "inode[" << index << "]"
        << " tid=" << inode.t_id
        << " name=" << inode_name(inode)
        << " size=" << inode.filesize
        << " blocks=" << char_to_binary(inode.blocks);
    return row.str();
}

void write_text_dump(const std::filesystem::path& output_path, const std::vector<std::string>& lines) {
    std::ofstream output(output_path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to create text dump: " + output_path.string());
    }

    for (const auto& line : lines) {
        output << line << '\n';
    }
}

void logLines(TaskDashboard& dashboard, const int taskId, const std::vector<std::string>& lines) {
    for (const auto& line : lines) {
        dashboard.logTask(taskId, line);
    }
}

void logTable(TaskDashboard& dashboard, const std::string& title, const std::vector<std::string>& lines) {
    dashboard.logQueue("");
    dashboard.logQueue(title);
    for (const auto& line : lines) {
        dashboard.logQueue(line);
    }
}

void runLab16(TaskDashboard& dashboard) {
    const std::filesystem::path filePath = std::filesystem::current_path() / "i-nodes";
    const std::filesystem::path textDumpPath = std::filesystem::current_path() / "i-node-dump.txt";
    const int i_node_size = static_cast<int>(sizeof(i_node));
    const i_node defaultNode = make_i_node("empty", -1, -1, 0x00);

    dashboard.logSystem("Lab 16 inode dashboard online.");
    dashboard.logSystem("Binary inode file: " + filePath.string());
    dashboard.logSystem("Text dump artifact: " + textDumpPath.string());
    dashboard.logTask(0, "Step 1: initialize 8 inode slots with the default empty record.");

    init_inode(filePath, kInodeCount, i_node_size, defaultNode);
    logTable(dashboard, "Snapshot after initialization", dump_inode(filePath));
    dashboard.logTask(1, "Raw bytes after initialization");
    logLines(dashboard, 1, dump_inode(filePath, i_node_size));

    const i_node file1 = make_i_node("file1", 1, 50, 0x80);
    dashboard.logTask(0, "Step 2: write file1 to slot 0.");
    write_inode(filePath, inode_offset(0), file1, i_node_size);

    const i_node file2 = make_i_node("file2", 2, 150, 0x60);
    dashboard.logTask(0, "Step 3: write file2 to slot 3.");
    write_inode(filePath, inode_offset(3), file2, i_node_size);

    logTable(dashboard, "Snapshot after writes", dump_inode(filePath));
    dashboard.logTask(1, "Raw bytes after writes");
    logLines(dashboard, 1, dump_inode(filePath, i_node_size));

    i_node readBuffer{};
    dashboard.logTask(2, "Step 4: read slot 2 (expected empty).");
    read_inode(filePath, inode_offset(2), readBuffer, i_node_size);
    dashboard.logTask(2, format_read_result(2, readBuffer));

    dashboard.logTask(2, "Step 5: read slot 0 (expected file1).");
    read_inode(filePath, inode_offset(0), readBuffer, i_node_size);
    dashboard.logTask(2, format_read_result(0, readBuffer));

    dashboard.logTask(2, "Step 6: read slot 3 (expected file2).");
    read_inode(filePath, inode_offset(3), readBuffer, i_node_size);
    dashboard.logTask(2, format_read_result(3, readBuffer));

    const auto finalDump = dump_inode(filePath);
    logTable(dashboard, "Final inode table", finalDump);
    write_text_dump(textDumpPath, finalDump);
    dashboard.logTask(0, "Submission text dump written to " + textDumpPath.string());
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
