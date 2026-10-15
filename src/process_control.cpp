#include "process_control.hpp"
#include <signal.h>
#include <sys/resource.h>

#include <cerrno>
#include <cstring>
#include <string>

static ControlResult signalProcess(
    int pid,
    int signal,
    const std::string& action
) {
    if (kill(pid, signal) == 0) {
        return {
            true,
            action + " aplicada al PID "
            + std::to_string(pid)
        };
    }

    if (errno == ESRCH) {
        return {
            false,
            "El proceso ya no existe."
        };
    }

    if (errno == EPERM) {
        return {
            false,
            "Sin permiso para actuar sobre ese proceso."
        };
    }

    return {
        false,
        std::string("Error: ") + std::strerror(errno)
    };
}

ControlResult terminateProcess(int pid) {
    return signalProcess(
        pid,
        SIGTERM,
        "SIGTERM"
    );
}

ControlResult forceKillProcess(int pid) {
    return signalProcess(
        pid,
        SIGKILL,
        "SIGKILL"
    );
}

ControlResult suspendProcess(int pid) {
    return signalProcess(
        pid,
        SIGSTOP,
        "Proceso suspendido"
    );
}

ControlResult resumeProcess(int pid) {
    return signalProcess(
        pid,
        SIGCONT,
        "Proceso reanudado"
    );
}

ControlResult changeProcessPriority(
    int pid,
    int priority
) {
    if (priority < -20 || priority > 19) {
        return {
            false,
            "La prioridad debe estar entre -20 y 19."
        };
    }

    errno = 0;

    if (
        setpriority(
            PRIO_PROCESS,
            pid,
            priority
        ) == -1
    ) {
        if (errno == ESRCH) {
            return {
                false,
                "El proceso ya no existe."
            };
        }

        if (
            errno == EACCES ||
            errno == EPERM
        ) {
            return {
                false,
                "Sin permiso para cambiar prioridad."
            };
        }

        return {
            false,
            std::string("Error: ")
            + std::strerror(errno)
        };
    }

    return {
        true,
        "Prioridad del PID "
        + std::to_string(pid)
        + " cambiada a "
        + std::to_string(priority)
    };
}