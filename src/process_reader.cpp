#include "../include/process_reader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iterator>
#include <cctype>
#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

static bool isNumber(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

static long readTotalMemoryKB() {
    std::ifstream file("/proc/meminfo");

    std::string key;
    long value;
    std::string unit;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") {
            return value;
        }
    }

    return 0;
}

static std::string getUserName(unsigned int uid) {
    struct passwd* pwd = getpwuid(uid);

    if (pwd != nullptr) {
        return pwd->pw_name;
    }

    return std::to_string(uid);
}

static std::string readCommand(int pid) {
    std::ifstream file(
        "/proc/" + std::to_string(pid) + "/cmdline",
        std::ios::binary
    );

    if (!file) {
        return "";
    }

    std::string command(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    for (char& c : command) {
        if (c == '\0') {
            c = ' ';
        }
    }

    return command;
}

static void readStatus(ProcessInfo& process) {
    std::ifstream file(
        "/proc/" + std::to_string(process.pid) + "/status"
    );

    if (!file) {
        return;
    }

    std::string key;

    while (file >> key) {

        if (key == "VmRSS:") {
            file >> process.rssKB;
        }

        else if (key == "Threads:") {
            file >> process.threads;
        }

        else if (key == "Uid:") {
            unsigned int uid;
            file >> uid;

            process.user = getUserName(uid);
        }

        std::string rest;
        std::getline(file, rest);
    }
}

static bool readStat(ProcessInfo& process) {
    std::ifstream file(
        "/proc/" + std::to_string(process.pid) + "/stat"
    );

    if (!file) {
        return false;
    }

    std::string line;
    std::getline(file, line);

    std::size_t left = line.find('(');
    std::size_t right = line.rfind(')');

    if (left == std::string::npos ||
        right == std::string::npos) {
        return false;
    }

    process.name =
        line.substr(left + 1, right - left - 1);

    std::string remaining =
        line.substr(right + 2);

    std::istringstream iss(remaining);

    iss >> process.state;
    iss >> process.ppid;

    long dummy = 0;

    // Campos 5 al 13
    for (int i = 0; i < 9; i++) {
        iss >> dummy;
    }

    // Campos 14 y 15
    iss >> process.utime;
    iss >> process.stime;

    // Campos 16 y 17
    iss >> dummy;
    iss >> dummy;

    // Campo 18
    iss >> process.priority;

    return true;
}

std::vector<ProcessInfo> readProcesses() {
    std::vector<ProcessInfo> processes;

    long totalMemoryKB = readTotalMemoryKB();
    long ticksPerSecond = sysconf(_SC_CLK_TCK);

    for (const auto& entry : fs::directory_iterator("/proc")) {

        if (!entry.is_directory()) {
            continue;
        }

        std::string filename =
            entry.path().filename().string();

        if (!isNumber(filename)) {
            continue;
        }

        ProcessInfo process;
        process.pid = std::stoi(filename);

        if (!readStat(process)) {
            continue;
        }

        readStatus(process);

        process.command =
            readCommand(process.pid);

        if (process.command.empty()) {
            process.command = process.name;
        }

        if (totalMemoryKB > 0) {
            process.memPercent =
                (static_cast<double>(process.rssKB) /
                 totalMemoryKB) * 100.0;
        }

        if (ticksPerSecond > 0) {
            process.cpuTimeSeconds =
                static_cast<double>(
                    process.utime + process.stime
                ) / ticksPerSecond;
        }

        processes.push_back(process);
    }

    return processes;
}