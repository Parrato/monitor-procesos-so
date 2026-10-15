#include "../include/system_reader.hpp"

#include <fstream>
#include <string>

static void readLoadAverage(SystemInfo& info) {
    std::ifstream file("/proc/loadavg");

    if (!file) {
        return;
    }

    file >> info.load1
         >> info.load5
         >> info.load15;
}

static void readMemoryInfo(SystemInfo& info) {
    std::ifstream file("/proc/meminfo");

    if (!file) {
        return;
    }

    std::string key;

    while (file >> key) {
        if (key == "MemTotal:") {
            file >> info.memTotalKB;
        }
        else if (key == "MemAvailable:") {
            file >> info.memAvailableKB;
        }

        std::string rest;
        std::getline(file, rest);
    }

    info.memUsedKB =
        info.memTotalKB - info.memAvailableKB;
}

SystemInfo readSystemInfo(
    const std::vector<ProcessInfo>& processes
) {
    SystemInfo info;

    info.totalProcesses =
        static_cast<int>(processes.size());

    for (const auto& process : processes) {

        if (process.state == 'R') {
            info.runningProcesses++;
        }
        else if (
            process.state == 'T' ||
            process.state == 't'
        ) {
            info.stoppedProcesses++;
        }
        else if (process.state == 'Z') {
            info.zombieProcesses++;
        }
    }

    readLoadAverage(info);
    readMemoryInfo(info);

    return info;
}