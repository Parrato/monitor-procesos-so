#include "../include/tui.hpp"
#include "../include/process_reader.hpp"
#include "../include/system_reader.hpp"
#include "../include/process_control.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

enum class SortMode {
    CPU,
    MEMORY
};

struct ThreadInfo {
    int tid = 0;
    std::string name;
    char state = '?';
};

// --------------------------------------------------
// CPU
// --------------------------------------------------

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

        auto it =
            previousCpu.find(p.pid);

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

// --------------------------------------------------
// FILTRO Y ORDENAMIENTO
// --------------------------------------------------

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
        return text.substr(
            0,
            maxLength
        );
    }

    return text.substr(
        0,
        maxLength - 3
    ) + "...";
}

// --------------------------------------------------
// HISTORICO CPU
// --------------------------------------------------

static void updateHistory(
    const std::vector<ProcessInfo>& processes,
    std::unordered_map<int, std::deque<double>>& history
) {
    const std::size_t MAX_HISTORY = 30;

    for (const auto& p : processes) {

        auto& samples =
            history[p.pid];

        samples.push_back(
            p.cpuPercent
        );

        if (
            samples.size()
            > MAX_HISTORY
        ) {
            samples.pop_front();
        }
    }
}

static void showHistory(
    const ProcessInfo& process,
    const std::unordered_map<
        int,
        std::deque<double>
    >& history
) {
    erase();

    int height;
    int width;

    getmaxyx(
        stdscr,
        height,
        width
    );

    mvprintw(
        0,
        0,
        "HISTORICO CPU - PID %d - %s",
        process.pid,
        process.name.c_str()
    );

    mvhline(
        1,
        0,
        ACS_HLINE,
        width
    );

    auto it =
        history.find(process.pid);

    if (
        it == history.end()
        ||
        it->second.empty()
    ) {
        mvprintw(
            3,
            0,
            "Todavia no hay suficientes muestras."
        );
    }
    else {

        const auto& samples =
            it->second;

        int row = 3;

        int barMax =
            std::max(
                10,
                width - 20
            );

        for (
            double value : samples
        ) {
            if (
                row >= height - 2
            ) {
                break;
            }

            int bars =
                static_cast<int>(
                    value
                );

            if (bars > barMax) {
                bars = barMax;
            }

            mvprintw(
                row,
                0,
                "%6.2f%% | ",
                value
            );

            for (
                int i = 0;
                i < bars;
                i++
            ) {
                addch('#');
            }

            row++;
        }
    }

    mvprintw(
        height - 1,
        0,
        "Presiona cualquier tecla para volver."
    );

    refresh();

    timeout(-1);
    getch();
}

// --------------------------------------------------
// HILOS
// --------------------------------------------------

static bool isNumeric(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (char c : value) {
        if (
            !std::isdigit(
                static_cast<unsigned char>(c)
            )
        ) {
            return false;
        }
    }

    return true;
}

static std::vector<ThreadInfo>
readThreads(int pid) {

    std::vector<ThreadInfo>
        threads;

    std::string taskPath =
        "/proc/"
        + std::to_string(pid)
        + "/task";

    try {

        for (
            const auto& entry :
            fs::directory_iterator(
                taskPath
            )
        ) {

            std::string id =
                entry.path()
                    .filename()
                    .string();

            if (!isNumeric(id)) {
                continue;
            }

            ThreadInfo info;

            info.tid =
                std::stoi(id);

            std::ifstream file(
                entry.path()
                / "status"
            );

            std::string key;

            while (
                file >> key
            ) {

                if (key == "Name:") {
                    file >> info.name;
                }

                else if (
                    key == "State:"
                ) {
                    file >> info.state;
                }

                std::string rest;
                std::getline(
                    file,
                    rest
                );
            }

            threads.push_back(
                info
            );
        }
    }
    catch (...) {
        // El proceso puede desaparecer
        // mientras consultamos sus hilos.
    }

    std::sort(
        threads.begin(),
        threads.end(),
        [](const ThreadInfo& a,
           const ThreadInfo& b) {

            return a.tid < b.tid;
        }
    );

    return threads;
}

static void showThreads(
    const ProcessInfo& process
) {
    auto threads =
        readThreads(
            process.pid
        );

    erase();

    int height;
    int width;

    getmaxyx(
        stdscr,
        height,
        width
    );

    mvprintw(
        0,
        0,
        "HILOS - PID %d - %s",
        process.pid,
        process.name.c_str()
    );

    mvprintw(
        1,
        0,
        "Total de hilos: %zu",
        threads.size()
    );

    mvhline(
        2,
        0,
        ACS_HLINE,
        width
    );

    mvprintw(
        3,
        0,
        "%-10s %-10s %s",
        "TID",
        "ESTADO",
        "NOMBRE"
    );

    int row = 4;

    for (
        const auto& thread :
        threads
    ) {
        if (
            row >= height - 2
        ) {
            break;
        }

        mvprintw(
            row,
            0,
            "%-10d %-10c %s",
            thread.tid,
            thread.state,
            thread.name.c_str()
        );

        row++;
    }

    if (threads.empty()) {

        mvprintw(
            5,
            0,
            "No se pudieron leer los hilos o el proceso ya termino."
        );
    }

    mvprintw(
        height - 1,
        0,
        "Presiona cualquier tecla para volver."
    );

    refresh();

    timeout(-1);
    getch();
}

// --------------------------------------------------
// ARBOL DE PROCESOS
// --------------------------------------------------

static void showProcessTree(
    const std::vector<ProcessInfo>& processes
) {
    erase();

    int height;
    int width;

    getmaxyx(
        stdscr,
        height,
        width
    );

    mvprintw(
        0,
        0,
        "ARBOL DE PROCESOS"
    );

    mvhline(
        1,
        0,
        ACS_HLINE,
        width
    );

    std::unordered_map<
        int,
        const ProcessInfo*
    > byPid;

    std::unordered_map<
        int,
        std::vector<const ProcessInfo*>
    > children;

    for (
        const auto& p :
        processes
    ) {
        byPid[p.pid] = &p;

        children[p.ppid]
            .push_back(&p);
    }

    for (
        auto& pair :
        children
    ) {
        std::sort(
            pair.second.begin(),
            pair.second.end(),
            [](const ProcessInfo* a,
               const ProcessInfo* b) {

                return a->pid < b->pid;
            }
        );
    }

    std::vector<
        const ProcessInfo*
    > roots;

    for (
        const auto& p :
        processes
    ) {
        if (
            p.ppid == 0
            ||
            byPid.find(p.ppid)
                == byPid.end()
        ) {
            roots.push_back(&p);
        }
    }

    int row = 2;

    std::unordered_set<int>
        visited;

    std::function<void(
        const ProcessInfo*,
        int
    )> renderNode;

    renderNode =
        [&](const ProcessInfo* p,
            int depth) {

        if (
            row >= height - 2
        ) {
            return;
        }

        if (
            visited.find(p->pid)
            != visited.end()
        ) {
            return;
        }

        visited.insert(p->pid);

        std::string prefix(
            depth * 2,
            ' '
        );

        std::string line =
            prefix
            + (depth > 0
                ? "|- "
                : "")
            + std::to_string(
                p->pid
            )
            + " "
            + p->name;

        mvprintw(
            row,
            0,
            "%s",
            shorten(
                line,
                width - 1
            ).c_str()
        );

        row++;

        auto it =
            children.find(
                p->pid
            );

        if (
            it != children.end()
        ) {
            for (
                const auto* child :
                it->second
            ) {
                renderNode(
                    child,
                    depth + 1
                );
            }
        }
    };

    for (
        const auto* root :
        roots
    ) {
        renderNode(
            root,
            0
        );

        if (
            row >= height - 2
        ) {
            break;
        }
    }

    mvprintw(
        height - 1,
        0,
        "Presiona cualquier tecla para volver."
    );

    refresh();

    timeout(-1);
    getch();
}

// --------------------------------------------------
// DIBUJO PRINCIPAL
// --------------------------------------------------

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
    const int footerRows = 4;

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

        if (
            index == selected
        ) {
            attron(
                A_REVERSE
            );
        }

        if (
            p.state == 'Z'
            ||
            p.state == 'D'
        ) {
            attron(
                A_BOLD
            );
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
            p.state == 'Z'
            ||
            p.state == 'D'
        ) {
            attroff(
                A_BOLD
            );
        }

        if (
            index == selected
        ) {
            attroff(
                A_REVERSE
            );
        }
    }

    mvhline(
        height - 4,
        0,
        ACS_HLINE,
        width
    );

    mvprintw(
        height - 3,
        0,
        "%s",
        message.c_str()
    );

    mvprintw(
        height - 2,
        0,
        "t Arbol | h Hilos | g Historico | k Term | K Kill | s Stop | r Resume | p Prioridad"
    );

    mvprintw(
        height - 1,
        0,
        "q Salir | Flechas Mover | c CPU | m MEM | f Filtro | +/- Refresco"
    );

    refresh();
}

// --------------------------------------------------
// ENTRADA Y CONFIRMACIONES
// --------------------------------------------------

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

    return std::string(
        buffer
    );
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

    int key =
        getch();

    return (
        key == 'y'
        ||
        key == 'Y'
    );
}

// --------------------------------------------------
// LOOP PRINCIPAL
// --------------------------------------------------

void runTui() {

    initscr();

    cbreak();
    noecho();

    keypad(
        stdscr,
        TRUE
    );

    curs_set(0);

    int refreshMs =
        1000;

    SortMode sortMode =
        SortMode::CPU;

    std::string filter;

    std::string message =
        "Monitor iniciado.";

    int selected = 0;

    std::unordered_map<
        int,
        std::deque<double>
    > cpuHistory;

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

        updateHistory(
            currentProcesses,
            cpuHistory
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

        if (
            visibleProcesses.empty()
        ) {
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

        timeout(
            refreshMs
        );

        int key =
            getch();

        if (key == 'q') {
            running = false;
        }

        else if (
            key == KEY_UP
        ) {
            if (selected > 0) {
                selected--;
            }
        }

        else if (
            key == KEY_DOWN
        ) {
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

            if (
                refreshMs < 250
            ) {
                refreshMs = 250;
            }

            message =
                "Intervalo reducido.";
        }

        else if (key == '-') {

            refreshMs += 250;

            if (
                refreshMs > 5000
            ) {
                refreshMs = 5000;
            }

            message =
                "Intervalo aumentado.";
        }

        else if (key == 't') {

            showProcessTree(
                currentProcesses
            );

            message =
                "Vista de arbol cerrada.";
        }

        else if (
            !visibleProcesses.empty()
        ) {
            const ProcessInfo&
                process =
                    visibleProcesses[
                        selected
                    ];

            ControlResult result{
                false,
                ""
            };

            if (key == 'h') {

                showThreads(
                    process
                );

                message =
                    "Vista de hilos cerrada.";
            }

            else if (key == 'g') {

                showHistory(
                    process,
                    cpuHistory
                );

                message =
                    "Historico cerrado.";
            }

            else if (key == 'k') {

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
                        std::stoi(
                            value
                        );

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