#include "../include/process_reader.hpp"

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_map>

int main() {

    // Primera lectura
    auto previousProcesses = readProcesses();
    unsigned long long previousTotalCpu =
        readTotalCpuTime();

    // Esperamos para medir cuánto cambian los contadores
    std::this_thread::sleep_for(
        std::chrono::seconds(1)
    );

    // Segunda lectura
    auto processes = readProcesses();
    unsigned long long currentTotalCpu =
        readTotalCpuTime();

    unsigned long long totalDelta =
        currentTotalCpu - previousTotalCpu;

    // PID -> tiempo de CPU anterior
    std::unordered_map<int, unsigned long long>
        previousCpu;

    for (const auto& p : previousProcesses) {
        previousCpu[p.pid] =
            p.utime + p.stime;
    }

    // Calculamos %CPU
    for (auto& p : processes) {

        auto it = previousCpu.find(p.pid);

        if (it != previousCpu.end() &&
            totalDelta > 0) {

            unsigned long long currentProcessCpu =
                p.utime + p.stime;

            unsigned long long processDelta =
                currentProcessCpu - it->second;

            p.cpuPercent =
                (static_cast<double>(processDelta) /
                 static_cast<double>(totalDelta))
                * 100.0;
        }
    }

    std::cout
        << std::left
        << std::setw(7)  << "PID"
        << std::setw(7)  << "PPID"
        << std::setw(12) << "USUARIO"
        << std::setw(8)  << "ESTADO"
        << std::setw(9)  << "%CPU"
        << std::setw(9)  << "%MEM"
        << std::setw(12) << "RAM(KB)"
        << std::setw(8)  << "HILOS"
        << std::setw(10) << "PRIO"
        << std::setw(12) << "CPU(s)"
        << "COMANDO\n";

    std::cout
        << "-------------------------------------------------------------"
        << "--------------------------------------------------\n";

    for (const auto& p : processes) {

        std::cout
            << std::left
            << std::setw(7)  << p.pid
            << std::setw(7)  << p.ppid
            << std::setw(12) << p.user
            << std::setw(8)  << p.state

            << std::setw(9)
            << std::fixed
            << std::setprecision(2)
            << p.cpuPercent

            << std::setw(9)
            << p.memPercent

            << std::setw(12)
            << p.rssKB

            << std::setw(8)
            << p.threads

            << std::setw(10)
            << p.priority

            << std::setw(12)
            << p.cpuTimeSeconds

            << p.command
            << '\n';
    }

    std::cout
        << "\nProcesos encontrados: "
        << processes.size()
        << '\n';

    return 0;
}