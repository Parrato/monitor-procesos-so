#include "../include/process_reader.hpp"
#include "../include/system_reader.hpp"

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_map>

static double kbToGB(long kb) {
    return static_cast<double>(kb)
        / 1024.0
        / 1024.0;
}

int main() {

    // Primera lectura para calcular %CPU
    auto previousProcesses = readProcesses();
    unsigned long long previousTotalCpu =
        readTotalCpuTime();

    std::this_thread::sleep_for(
        std::chrono::seconds(1)
    );

    // Segunda lectura
    auto processes = readProcesses();
    unsigned long long currentTotalCpu =
        readTotalCpuTime();

    unsigned long long totalDelta =
        currentTotalCpu - previousTotalCpu;

    std::unordered_map<int, unsigned long long>
        previousCpu;

    for (const auto& p : previousProcesses) {
        previousCpu[p.pid] =
            p.utime + p.stime;
    }

    for (auto& p : processes) {

        auto it = previousCpu.find(p.pid);

        if (
            it != previousCpu.end() &&
            totalDelta > 0
        ) {
            unsigned long long currentProcessCpu =
                p.utime + p.stime;

            unsigned long long processDelta =
                currentProcessCpu - it->second;

            p.cpuPercent =
                (
                    static_cast<double>(processDelta)
                    /
                    static_cast<double>(totalDelta)
                ) * 100.0;
        }
    }

    SystemInfo system =
        readSystemInfo(processes);

    std::cout
        << "\n================ MONITOR DE PROCESOS ================\n";

    std::cout
        << "Procesos: "
        << system.totalProcesses
        << " | Running: "
        << system.runningProcesses
        << " | Detenidos: "
        << system.stoppedProcesses
        << " | Zombies: "
        << system.zombieProcesses
        << '\n';

    std::cout
        << std::fixed
        << std::setprecision(2)
        << "Load Average: "
        << system.load1 << " "
        << system.load5 << " "
        << system.load15
        << '\n';

    std::cout
        << "RAM: "
        << kbToGB(system.memUsedKB)
        << " GB usada / "
        << kbToGB(system.memTotalKB)
        << " GB total | "
        << kbToGB(system.memAvailableKB)
        << " GB disponible\n";

    std::cout
        << "======================================================\n\n";

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

    return 0;
}