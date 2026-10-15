#ifndef SYSTEM_READER_HPP
#define SYSTEM_READER_HPP

#include "process_reader.hpp"

#include <vector>

struct SystemInfo {
    int totalProcesses = 0;
    int runningProcesses = 0;
    int stoppedProcesses = 0;
    int zombieProcesses = 0;

    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;

    long memTotalKB = 0;
    long memUsedKB = 0;
    long memAvailableKB = 0;
};

SystemInfo readSystemInfo(
    const std::vector<ProcessInfo>& processes
);

#endif