#ifndef PROCESS_READER_HPP
#define PROCESS_READER_HPP

#include <string>
#include <vector>

struct ProcessInfo {
    int pid = 0;
    int ppid = 0;

    std::string name;
    std::string user;
    char state = '?';

    long rssKB = 0;
    double memPercent = 0.0;

    int threads = 0;
    long priority = 0;

    unsigned long utime = 0;
    unsigned long stime = 0;
    double cpuTimeSeconds = 0.0;

    std::string command;
};

std::vector<ProcessInfo> readProcesses();

#endif