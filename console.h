#pragma once

#include "config.h"
#include "scheduler.h"

#include <string>

class Console {
private:
    Config config;
    Scheduler scheduler;
    bool initialized = false;
    bool running = true;

    void printWelcome() const;
    void printHelp() const;

    void executeCommand(const std::string& commandLine);
    void handleScreenCommand(const std::string& commandLine);
    void enterProcessScreen(const std::string& processName, bool newlyCreated = false);
    void printProcessInfo(const ProcessSnapshot& process) const;

    void clearConsole() const;
    std::string readLine(const std::string& prompt) const;

public:
    void run();
};