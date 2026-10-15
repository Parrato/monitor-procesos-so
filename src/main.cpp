#include "../include/process_reader.hpp"

#include <iostream>
#include <iomanip>

int main() {
    auto processes = readProcesses();

    std::cout
        << std::left
        << std::setw(7)  << "PID"
        << std::setw(7)  << "PPID"
        << std::setw(12) << "USUARIO"
        << std::setw(8)  << "ESTADO"
        << std::setw(9)  << "%MEM"
        << std::setw(12) << "RAM(KB)"
        << std::setw(8)  << "HILOS"
        << std::setw(10) << "PRIO"
        << std::setw(12) << "CPU(s)"
        << "COMANDO\n";

    std::cout
        << "--------------------------------------------------------------------------"
        << "----------------------------\n";

    for (const auto& p : processes) {

        std::cout
            << std::left
            << std::setw(7)  << p.pid
            << std::setw(7)  << p.ppid
            << std::setw(12) << p.user
            << std::setw(8)  << p.state
            << std::setw(9)  << std::fixed
            << std::setprecision(2) << p.memPercent
            << std::setw(12) << p.rssKB
            << std::setw(8)  << p.threads
            << std::setw(10) << p.priority
            << std::setw(12) << std::fixed
            << std::setprecision(2) << p.cpuTimeSeconds
            << p.command
            << '\n';
    }

    std::cout
        << "\nProcesos encontrados: "
        << processes.size()
        << '\n';

    return 0;
}