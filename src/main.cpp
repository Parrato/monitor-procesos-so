#include "../include/process_reader.hpp"

#include <iostream>
#include <iomanip>

int main() {
    auto processes = readProcesses();

    std::cout
        << std::left
        << std::setw(8)  << "PID"
        << std::setw(8)  << "PPID"
        << std::setw(10) << "ESTADO"
        << std::setw(10) << "HILOS"
        << std::setw(12) << "RAM(KB)"
        << std::setw(12) << "PRIORIDAD"
        << "COMANDO\n";

    std::cout
        << "--------------------------------------------------------------------------\n";

    for (const auto& p : processes) {
        std::cout
            << std::left
            << std::setw(8)  << p.pid
            << std::setw(8)  << p.ppid
            << std::setw(10) << p.state
            << std::setw(10) << p.threads
            << std::setw(12) << p.rssKB
            << std::setw(12) << p.priority
            << p.command
            << '\n';
    }

    std::cout << "\nProcesos encontrados: "
              << processes.size()
              << '\n';

    return 0;
}