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
    long value = 0;
    std::string unit;

    while (file >> key >> value >> unit) {

        if (key == "MemTotal:") {
            info.memTotalKB = value;
        }

        else if (key == "MemAvailable:") {
            info.memAvailableKB = value;
        }
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