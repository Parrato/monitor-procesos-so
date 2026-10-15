#ifndef PROCESS_CONTROL_HPP
#define PROCESS_CONTROL_HPP

#include <string>

struct ControlResult {
    bool success;
    std::string message;
};

ControlResult terminateProcess(int pid);
ControlResult forceKillProcess(int pid);
ControlResult suspendProcess(int pid);
ControlResult resumeProcess(int pid);
ControlResult changeProcessPriority(int pid, int priority);

#endif