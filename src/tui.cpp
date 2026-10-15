#include "../include/tui.hpp"
#include "../include/process_reader.hpp"
#include "../include/system_reader.hpp"
#include "../include/process_control.hpp"

#include <ncurses.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

enum class SortMode {
    CPU,
    MEMORY
};

static void calculateCpuPercent(
    std::vector<ProcessInfo>& current,
    const std::vector<ProcessInfo>& previous,
    unsigned long long currentTotal,
    unsigned long long previousTotal
) {
    if (currentTotal <= previousTotal) {
        return;
    }

    unsigned long long totalDelta =
        currentTotal - previousTotal;

    std::unordered_map<int, unsigned long long>
        previousCpu;

    for (const auto& p : previous) {
        previousCpu[p.pid] =
            p.utime + p.stime;
    }

    for (auto& p : current) {

        auto it = previousCpu.find(p.pid);

        if (it == previousCpu.end()) {
            continue;
        }

        unsigned long long currentProcessCpu =
            p.utime + p.stime;

        if (currentProcessCpu < it->second) {
            continue;
        }

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

static std::vector<ProcessInfo> filterProcesses(
    const std::vector<ProcessInfo>& processes,
    const std::string& filter
) {
    if (filter.empty()) {
        return processes;
    }

    std::vector<ProcessInfo> result;

    for (const auto& p : processes) {
        if (
            p.name.find(filter)
                != std::string::npos
            ||
            p.command.find(filter)
                != std::string::npos
        ) {
            result.push_back(p);
        }
    }

    return result;
}

static void sortProcesses(
    std::vector<ProcessInfo>& processes,
    SortMode mode
) {
    if (mode == SortMode::CPU) {

        std::sort(
            processes.begin(),
            processes.end(),
            [](const ProcessInfo& a,
               const ProcessInfo& b) {
                return a.cpuPercent
                    > b.cpuPercent;
            }
        );
    }
    else {

        std::sort(
            processes.begin(),
            processes.end(),
            [](const ProcessInfo& a,
               const ProcessInfo& b) {
                return a.rssKB > b.rssKB;
            }
        );
    }
}

static std::string shorten(
    const std::string& text,
    int maxLength
) {
    if (maxLength <= 0) {
        return "";
    }

    if (
        static_cast<int>(text.size())
        <= maxLength
    ) {
        return text;
    }

    if (maxLength <= 3) {
        return text.substr(0, maxLength);
    }

    return text.substr(
        0,
        maxLength - 3
    ) + "...";
}

static void drawScreen(
    const std::vector<ProcessInfo>& processes,
    const SystemInfo& system,
    int selected,
    SortMode sortMode,
    const std::string& filter,
    int refreshMs,
    const std::string& message
) {
    erase();

    int height;
    int width;

    getmaxyx(
        stdscr,
        height,
        width
    );

    double totalGB =
        static_cast<double>(
            system.memTotalKB
        ) / 1024.0 / 1024.0;

    double usedGB =
        static_cast<double>(
            system.memUsedKB
        ) / 1024.0 / 1024.0;

    double availableGB =
        static_cast<double>(
            system.memAvailableKB
        ) / 1024.0 / 1024.0;

    mvprintw(
        0,
        0,
        "MONITOR | Total:%d Run:%d Stop:%d Zombie:%d",
        system.totalProcesses,
        system.runningProcesses,
        system.stoppedProcesses,
        system.zombieProcesses
    );

    mvprintw(
        1,
        0,
        "Load %.2f %.2f %.2f | RAM %.2f/%.2f GB | Disp %.2f GB",
        system.load1,
        system.load5,
        system.load15,
        usedGB,
        totalGB,
        availableGB
    );

    mvprintw(
        2,
        0,
        "Orden:%s | Refresco:%.2fs | Filtro:%s",
        sortMode == SortMode::CPU
            ? "CPU"
            : "MEM",
        refreshMs / 1000.0,
        filter.empty()
            ? "(ninguno)"
            : filter.c_str()
    );

    mvhline(
        3,
        0,
        ACS_HLINE,
        width
    );

    mvprintw(
        4,
        0,
        "%-6s %-6s %-10s %-2s %7s %7s %9s %5s %5s %8s %s",
        "PID",
        "PPID",
        "USER",
        "S",
        "CPU%",
        "MEM%",
        "RSS(KB)",
        "THR",
        "PRIO",
        "CPU(s)",
        "COMMAND"
    );

    const int firstTableRow = 5;
    const int footerRows = 3;

    int availableRows =
        height
        - firstTableRow
        - footerRows;

    if (availableRows < 1) {
        availableRows = 1;
    }

    int start = 0;

    if (selected >= availableRows) {
        start =
            selected
            - availableRows
            + 1;
    }

    for (
        int screenRow = 0;
        screenRow < availableRows;
        screenRow++
    ) {
        int index =
            start + screenRow;

        if (
            index >=
            static_cast<int>(
                processes.size()
            )
        ) {
            break;
        }

        const auto& p =
            processes[index];

        int row =
            firstTableRow
            + screenRow;

        if (index == selected) {
            attron(A_REVERSE);
        }

        if (
            p.state == 'Z' ||
            p.state == 'D'
        ) {
            attron(A_BOLD);
        }

        int commandWidth =
            std::max(
                10,
                width - 83
            );

        std::string command =
            shorten(
                p.command,
                commandWidth
            );

        mvprintw(
            row,
            0,
            "%-6d %-6d %-10.10s %-2c %7.2f %7.2f %9ld %5d %5ld %8.1f %s",
            p.pid,
            p.ppid,
            p.user.c_str(),
            p.state,
            p.cpuPercent,
            p.memPercent,
            p.rssKB,
            p.threads,
            p.priority,
            p.cpuTimeSeconds,
            command.c_str()
        );

        if (
            p.state == 'Z' ||
            p.state == 'D'
        ) {
            attroff(A_BOLD);
        }

        if (index == selected) {
            attroff(A_REVERSE);
        }
    }

    mvhline(
        height - 3,
        0,
        ACS_HLINE,
        width
    );

    mvprintw(
        height - 2,
        0,
        "%s",
        message.c_str()
    );

    mvprintw(
        height - 1,
        0,
        "q Salir | c CPU | m MEM | f Filtro | +/- Refresco | k Term | K Kill | s Stop | r Resume | p Prioridad"
    );

    refresh();
}

static std::string askText(
    const std::string& prompt
) {
    int height;
    int width;

    getmaxyx(
        stdscr,
        height,
        width
    );

    echo();
    curs_set(1);
    timeout(-1);

    char buffer[128] = {};

    move(
        height - 1,
        0
    );

    clrtoeol();

    mvprintw(
        height - 1,
        0,
        "%s",
        prompt.c_str()
    );

    getnstr(
        buffer,
        sizeof(buffer) - 1
    );

    noecho();
    curs_set(0);

    return std::string(buffer);
}

static bool confirmAction(
    const ProcessInfo& process,
    const std::string& action
) {
    int height;
    int width;

    getmaxyx(
        stdscr,
        height,
        width
    );

    timeout(-1);

    move(
        height - 1,
        0
    );

    clrtoeol();

    mvprintw(
        height - 1,
        0,
        "%s PID %d (%s)? [y/N]: ",
        action.c_str(),
        process.pid,
        process.name.c_str()
    );

    int key = getch();

    return (
        key == 'y' ||
        key == 'Y'
    );
}

void runTui() {

    initscr();

    cbreak();
    noecho();
    keypad(
        stdscr,
        TRUE
    );

    curs_set(0);

    int refreshMs = 1000;

    SortMode sortMode =
        SortMode::CPU;

    std::string filter;
    std::string message =
        "Monitor iniciado.";

    int selected = 0;

    auto previousProcesses =
        readProcesses();

    unsigned long long
        previousTotalCpu =
            readTotalCpuTime();

    bool running = true;

    while (running) {

        auto currentProcesses =
            readProcesses();

        unsigned long long
            currentTotalCpu =
                readTotalCpuTime();

        calculateCpuPercent(
            currentProcesses,
            previousProcesses,
            currentTotalCpu,
            previousTotalCpu
        );

        SystemInfo system =
            readSystemInfo(
                currentProcesses
            );

        auto visibleProcesses =
            filterProcesses(
                currentProcesses,
                filter
            );

        sortProcesses(
            visibleProcesses,
            sortMode
        );

        if (visibleProcesses.empty()) {
            selected = 0;
        }
        else if (
            selected >=
            static_cast<int>(
                visibleProcesses.size()
            )
        ) {
            selected =
                static_cast<int>(
                    visibleProcesses.size()
                ) - 1;
        }

        drawScreen(
            visibleProcesses,
            system,
            selected,
            sortMode,
            filter,
            refreshMs,
            message
        );

        timeout(refreshMs);

        int key = getch();

        if (key == 'q') {
            running = false;
        }

        else if (key == KEY_UP) {
            if (selected > 0) {
                selected--;
            }
        }

        else if (key == KEY_DOWN) {
            if (
                selected + 1
                <
                static_cast<int>(
                    visibleProcesses.size()
                )
            ) {
                selected++;
            }
        }

        else if (key == 'c') {
            sortMode =
                SortMode::CPU;

            selected = 0;

            message =
                "Ordenado por CPU.";
        }

        else if (key == 'm') {
            sortMode =
                SortMode::MEMORY;

            selected = 0;

            message =
                "Ordenado por memoria.";
        }

        else if (key == 'f') {
            filter =
                askText(
                    "Filtro (vacio = quitar): "
                );

            selected = 0;

            message =
                filter.empty()
                ? "Filtro eliminado."
                : "Filtro aplicado.";
        }

        else if (key == '+') {
            refreshMs -= 250;

            if (refreshMs < 250) {
                refreshMs = 250;
            }

            message =
                "Intervalo reducido.";
        }

        else if (key == '-') {
            refreshMs += 250;

            if (refreshMs > 5000) {
                refreshMs = 5000;
            }

            message =
                "Intervalo aumentado.";
        }

        else if (
            !visibleProcesses.empty()
        ) {
            const ProcessInfo& process =
                visibleProcesses[selected];

            ControlResult result{
                false,
                ""
            };

            if (key == 'k') {

                if (
                    confirmAction(
                        process,
                        "Terminar"
                    )
                ) {
                    result =
                        terminateProcess(
                            process.pid
                        );

                    message =
                        result.message;
                }
                else {
                    message =
                        "Accion cancelada.";
                }
            }

            else if (key == 'K') {

                if (
                    confirmAction(
                        process,
                        "FORZAR terminacion"
                    )
                ) {
                    result =
                        forceKillProcess(
                            process.pid
                        );

                    message =
                        result.message;
                }
                else {
                    message =
                        "Accion cancelada.";
                }
            }

            else if (key == 's') {

                if (
                    confirmAction(
                        process,
                        "Suspender"
                    )
                ) {
                    result =
                        suspendProcess(
                            process.pid
                        );

                    message =
                        result.message;
                }
                else {
                    message =
                        "Accion cancelada.";
                }
            }

            else if (key == 'r') {

                if (
                    confirmAction(
                        process,
                        "Reanudar"
                    )
                ) {
                    result =
                        resumeProcess(
                            process.pid
                        );

                    message =
                        result.message;
                }
                else {
                    message =
                        "Accion cancelada.";
                }
            }

            else if (key == 'p') {

                std::string value =
                    askText(
                        "Nueva prioridad (-20 a 19): "
                    );

                try {
                    int priority =
                        std::stoi(value);

                    result =
                        changeProcessPriority(
                            process.pid,
                            priority
                        );

                    message =
                        result.message;
                }
                catch (...) {
                    message =
                        "Prioridad invalida.";
                }
            }
        }

        previousProcesses =
            currentProcesses;

        previousTotalCpu =
            currentTotalCpu;
    }

    endwin();
}