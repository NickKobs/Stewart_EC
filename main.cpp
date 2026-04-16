#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <ncurses.h>
#include <unistd.h>

namespace {
constexpr std::size_t kBytesPerRow = 16;
constexpr int kHeaderHeight = 5;
constexpr int kFooterHeight = 2;
constexpr int kMinTerminalRows = 10;
constexpr int kMinTerminalCols = 80;

enum class UiMode {
    Auto,
    Ncurses,
    Text,
};

struct Options {
    UiMode uiMode = UiMode::Auto;
    bool showHelp = false;
    std::string path;
};

bool needsDefaultTerm() {
    const char* term = std::getenv("TERM");
    if (term == nullptr || term[0] == '\0') {
        return true;
    }

    const std::string_view termValue(term);
    return termValue == "unknown" || termValue == "dumb";
}

class NcursesSession {
public:
    NcursesSession() {
        if (needsDefaultTerm()) {
            setenv("TERM", "xterm-256color", 1);
        }

        if (initscr() == nullptr) {
            throw std::runtime_error("failed to initialize ncurses");
        }

        active_ = true;
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
        nodelay(stdscr, FALSE);
    }

    ~NcursesSession() {
        if (active_) {
            endwin();
        }
    }

    NcursesSession(const NcursesSession&) = delete;
    NcursesSession& operator=(const NcursesSession&) = delete;

private:
    bool active_ = false;
};

std::size_t calculateRowCount(const std::vector<std::uint8_t>& bytes) {
    return (bytes.size() + kBytesPerRow - 1) / kBytesPerRow;
}

int offsetWidth(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() > 0xFFFFFFFFULL ? 16 : 8;
}

std::string ellipsizeMiddle(const std::string& text, int maxWidth) {
    if (maxWidth <= 0) {
        return {};
    }

    if (static_cast<int>(text.size()) <= maxWidth) {
        return text;
    }

    if (maxWidth <= 3) {
        return text.substr(0, static_cast<std::size_t>(maxWidth));
    }

    const int prefixLength = (maxWidth - 3) / 2;
    const int suffixLength = maxWidth - 3 - prefixLength;
    return text.substr(0, static_cast<std::size_t>(prefixLength)) +
           "..." +
           text.substr(text.size() - static_cast<std::size_t>(suffixLength));
}

std::string formatHexLine(const std::vector<std::uint8_t>& bytes, std::size_t rowIndex) {
    const std::size_t start = rowIndex * kBytesPerRow;
    std::ostringstream line;
    line << std::uppercase << std::hex << std::setfill('0') << std::setw(offsetWidth(bytes)) << start << "  ";

    std::string ascii;
    ascii.reserve(kBytesPerRow);

    for (std::size_t i = 0; i < kBytesPerRow; ++i) {
        const std::size_t index = start + i;
        if (index < bytes.size()) {
            const unsigned int value = bytes[index];
            line << std::setw(2) << value << ' ';
            ascii.push_back(std::isprint(value) ? static_cast<char>(value) : '.');
        } else {
            line << "   ";
            ascii.push_back(' ');
        }

        if (i == 7) {
            line << ' ';
        }
    }

    line << " |" << ascii << '|';
    return line.str();
}

std::vector<std::uint8_t> loadFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to open file: " + path);
    }

    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
}

Options parseArguments(int argc, char* argv[]) {
    Options options;
    options.path = argc > 0 ? argv[0] : "";
    bool pathAssigned = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
            continue;
        }

        if (arg == "--text") {
            options.uiMode = UiMode::Text;
            continue;
        }

        if (arg == "--ncurses") {
            options.uiMode = UiMode::Ncurses;
            continue;
        }

        if (!arg.empty() && arg.front() == '-') {
            throw std::invalid_argument("unknown option: " + std::string(arg));
        }

        if (pathAssigned) {
            throw std::invalid_argument("only one input file may be provided");
        }

        options.path = std::string(arg);
        pathAssigned = true;
    }

    if (options.path.empty()) {
        throw std::invalid_argument("no executable path available for default input");
    }

    return options;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [--ncurses|--text] [file]\n";
    std::cout << "If no file is supplied, the program dumps its own executable.\n";
    std::cout << "In ncurses mode use Up/Down, PgUp/PgDn, Home/End, and q.\n";
}

void renderTranscript(const std::vector<std::uint8_t>& bytes, const std::string& path) {
    std::cout << "File: " << path << '\n';
    std::cout << "Size: " << bytes.size() << " bytes\n\n";

    if (bytes.empty()) {
        std::cout << "(empty file)\n";
        return;
    }

    const std::size_t rowCount = calculateRowCount(bytes);
    for (std::size_t row = 0; row < rowCount; ++row) {
        std::cout << formatHexLine(bytes, row) << '\n';
    }
}

void clampViewport(std::size_t& cursorRow, std::size_t& topRow, std::size_t totalRows, int visibleRows) {
    if (totalRows == 0) {
        cursorRow = 0;
        topRow = 0;
        return;
    }

    cursorRow = std::min(cursorRow, totalRows - 1);
    topRow = std::min(topRow, totalRows - 1);

    if (cursorRow < topRow) {
        topRow = cursorRow;
    }

    const std::size_t pageSize = static_cast<std::size_t>(std::max(1, visibleRows));
    if (cursorRow >= topRow + pageSize) {
        topRow = cursorRow - pageSize + 1;
    }
}

void renderNcursesViewer(const std::vector<std::uint8_t>& bytes, const std::string& path) {
    NcursesSession session;

    const std::size_t totalRows = calculateRowCount(bytes);
    std::size_t cursorRow = 0;
    std::size_t topRow = 0;

    while (true) {
        int maxY = 0;
        int maxX = 0;
        getmaxyx(stdscr, maxY, maxX);
        erase();
        box(stdscr, 0, 0);

        if (maxY < kMinTerminalRows || maxX < kMinTerminalCols) {
            mvaddnstr(1, 2, "Terminal too small for the hex viewer.", maxX - 4);
            mvaddnstr(2, 2, "Resize the window or press q to quit.", maxX - 4);
            refresh();

            const int key = getch();
            if (key == 'q' || key == 'Q' || key == 27) {
                return;
            }
            continue;
        }

        const int visibleRows = std::max(1, maxY - kHeaderHeight - kFooterHeight);
        clampViewport(cursorRow, topRow, totalRows, visibleRows);

        const std::string title = " Lab 17 File Hex Dump ";
        mvaddnstr(0, std::max(2, (maxX - static_cast<int>(title.size())) / 2), title.c_str(), static_cast<int>(title.size()));

        const std::string fileLine = "File: " + ellipsizeMiddle(path, maxX - 8);
        mvaddnstr(1, 2, fileLine.c_str(), maxX - 4);

        std::ostringstream meta;
        meta << "Size: " << bytes.size() << " bytes  |  Rows: " << totalRows << "  |  Offset width: "
             << offsetWidth(bytes) << " hex digits";
        const std::string metaLine = meta.str();
        mvaddnstr(2, 2, metaLine.c_str(), maxX - 4);

        mvaddnstr(3, 2, "Up/Down PgUp/PgDn Home/End scroll  |  q or Esc quits", maxX - 4);

        for (int screenRow = 0; screenRow < visibleRows; ++screenRow) {
            const std::size_t rowIndex = topRow + static_cast<std::size_t>(screenRow);
            if (rowIndex >= totalRows) {
                break;
            }

            const std::string line = formatHexLine(bytes, rowIndex);
            if (rowIndex == cursorRow) {
                attron(A_REVERSE);
            }

            mvaddnstr(kHeaderHeight + screenRow, 2, line.c_str(), maxX - 4);

            if (rowIndex == cursorRow) {
                attroff(A_REVERSE);
            }
        }

        std::ostringstream footer;
        footer << "Row " << (totalRows == 0 ? 0 : cursorRow + 1) << '/' << totalRows;
        if (totalRows != 0) {
            footer << "  |  Byte offset 0x"
                   << std::uppercase << std::hex << std::setfill('0') << std::setw(offsetWidth(bytes))
                   << (cursorRow * kBytesPerRow);
        }

        const std::string footerLine = footer.str();
        mvaddnstr(maxY - 2, std::max(2, maxX - static_cast<int>(footerLine.size()) - 2), footerLine.c_str(), static_cast<int>(footerLine.size()));

        refresh();

        const int key = getch();
        switch (key) {
            case 'q':
            case 'Q':
            case 27:
                return;
            case KEY_UP:
                if (cursorRow > 0) {
                    --cursorRow;
                }
                break;
            case KEY_DOWN:
                if (cursorRow + 1 < totalRows) {
                    ++cursorRow;
                }
                break;
            case KEY_PPAGE:
                if (cursorRow > static_cast<std::size_t>(visibleRows)) {
                    cursorRow -= static_cast<std::size_t>(visibleRows);
                } else {
                    cursorRow = 0;
                }
                break;
            case KEY_NPAGE:
                if (totalRows != 0) {
                    cursorRow = std::min(cursorRow + static_cast<std::size_t>(visibleRows), totalRows - 1);
                }
                break;
            case KEY_HOME:
                cursorRow = 0;
                break;
            case KEY_END:
                if (totalRows != 0) {
                    cursorRow = totalRows - 1;
                }
                break;
            case KEY_RESIZE:
                break;
            default:
                break;
        }
    }
}
}

int main(int argc, char* argv[]) {
    try {
        const Options options = parseArguments(argc, argv);
        if (options.showHelp) {
            printUsage(argc > 0 ? argv[0] : "Extra_Credit");
            return 0;
        }

        const std::vector<std::uint8_t> bytes = loadFile(options.path);
        const bool ttyReady = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

        if (options.uiMode != UiMode::Text && ttyReady) {
            try {
                renderNcursesViewer(bytes, options.path);
                return 0;
            } catch (const std::exception& error) {
                std::cerr << "[hex-dump] " << error.what() << " Falling back to transcript mode.\n";
            }
        } else if (options.uiMode == UiMode::Ncurses && !ttyReady) {
            std::cerr << "[hex-dump] ncurses requested but no compatible TTY was detected. ";
            std::cerr << "Using transcript mode instead.\n";
        }

        renderTranscript(bytes, options.path);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
