#include "../include/process_reader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iterator>
#include <cctype>

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

    // Campo 3: state
    iss >> process.state;

    // Campo 4: PPID
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

    // Campo 18: prioridad
    iss >> process.priority;

    return true;
}

std::vector<ProcessInfo> readProcesses() {
    std::vector<ProcessInfo> processes;

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

        // Un proceso puede desaparecer mientras lo estamos leyendo.
        if (!readStat(process)) {
            continue;
        }

        readStatus(process);

        process.command =
            readCommand(process.pid);

        if (process.command.empty()) {
            process.command = process.name;
        }

        processes.push_back(process);
    }

    return processes;
}