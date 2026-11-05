
#include "console.h"
#include "report.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

void Console::printWelcome() const {
    std::cout
        << "============================================================\n"
        << "  Group 8 | CSOPESY - M01 CPU Scheduler and CLI\n"
        << "============================================================\n"
        << "  Developers:\n"
        << "    Abenojar, Fred\n"
        << "    Caya, Mary Faye\n"
        << "    Diamante, Deo Zamir\n"
        << "    Guiller, Gerylyn\n"
        << "  Version Date: November 5, 2026\n"
        << "============================================================\n";
    if (initialized) {
        std::cout << "Type help for commands, or exit to quit.\n\n";
    }
    else {
        std::cout << "Type initialize first (or exit).\n"
                  << "After initialization, type help for commands.\n\n";
    }
}

void Console::printHelp() const {
    std::cout
        << "\nAvailable commands:\n"
        << "  initialize\n"
        << "  screen -s <process_name>\n"
        << "  screen -r <process_name>\n"
        << "  screen -ls\n"
        << "  scheduler-start\n"
        << "  scheduler-stop\n"
        << "  report-util\n"
        << "  help\n"
        << "  exit\n\n";
}

void Console::clearConsole() const {
    // Finish pending output before changing the display or moving its cursor.
    std::cout.flush();
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD originalMode = 0;
    // Redirected output is a transcript, not a screen to erase.
    if (!GetConsoleMode(console, &originalMode)) { return; }

    // Windows Terminal/ConPTY handles VT clearing consistently. Filling the
    // entire legacy scrollback buffer can leave stale or duplicated rows.
    if (SetConsoleMode(console, originalMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        constexpr char sequence[] = "\x1b[2J\x1b[H";
        constexpr DWORD length = sizeof(sequence) - 1;
        DWORD written = 0;
        const BOOL cleared = WriteConsoleA(console, sequence, length, &written, nullptr);
        SetConsoleMode(console, originalMode);
        if (cleared && written == length) { return; }
    }

    // Fallback for older consoles: clear only the visible rows, then home
    // within that window instead of jumping to the start of scrollback.
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(console, &info)) { return; }
    const COORD origin = { 0, info.srWindow.Top };
    const DWORD cells = static_cast<DWORD>(info.dwSize.X)
        * static_cast<DWORD>(info.srWindow.Bottom - info.srWindow.Top + 1);
    DWORD written = 0;
    FillConsoleOutputCharacterA(console, ' ', cells, origin, &written);
    FillConsoleOutputAttribute(console, info.wAttributes, cells, origin, &written);
    SetConsoleCursorPosition(console, origin);
#else
    std::cout << "\x1b[2J\x1b[H" << std::flush;
#endif
}

std::string Console::readLine(
    const std::string& prompt
) const {
    std::cout << prompt;

    std::string line;
    std::getline(std::cin, line);

    return line;
}

void Console::run() {
    clearConsole();
    printWelcome();

    while (running) {
        std::string prompt = initialized
            ? "CSOPESY> "
            : "CSOPESY (not initialized)> ";

        std::string commandLine = readLine(prompt);

        if (!std::cin) {
            break;
        }

        executeCommand(commandLine);
    }

    if (initialized) {
        scheduler.stop();
    }

    std::cout << "CSOPESY terminated.\n";
}

void Console::executeCommand(
    const std::string& commandLine
) {
    std::istringstream stream(commandLine);
    std::string command;

    stream >> command;

    if (command.empty()) {
        return;
    }

    if (command == "exit") {
        running = false;
        return;
    }

    if (command == "initialize") {
        if (initialized) {
            std::cout
                << "System is already initialized.\n";
            return;
        }

        std::string configError;
        if (!config.loadFromFile("config.txt", &configError)) {
            std::cout << "Failed to load config.txt: " << configError << '\n';
            return;
        }

        if (!scheduler.initialize(config)) {
            std::cout
                << "Failed to initialize scheduler.\n";
            return;
        }

        scheduler.start();
        initialized = true;

        std::cout
            << "Configuration loaded successfully.\n";

        std::cout
            << "CPUs: " << config.numCPU
            << ", scheduler: " << config.scheduler
            << '\n';

        return;
    }

    if (!initialized) {
        std::cout
            << "Please run initialize before using other commands.\n";
        return;
    }

    if (command == "help") {
        printHelp();
    }
    else if (command == "screen") {
        handleScreenCommand(commandLine);
    }
    else if (command == "scheduler-start") {
        scheduler.startBatchGeneration();

        std::cout
            << "Scheduler batch generation started.\n";
    }
    else if (command == "scheduler-stop") {
        scheduler.stopBatchGeneration();

        std::cout
            << "Scheduler batch generation stopped.\n";
    }
    else if (command == "report-util") {
        const std::string report = Report::generate(scheduler);
        std::cout << report;

        if (Report::save(report)) {
            std::cout
                << "Report saved to csopesy-log.txt\n";
        }
        else {
            std::cout
                << "Unable to write csopesy-log.txt\n";
        }
    }
    else {
        std::cout
            << "Unknown command: "
            << command
            << '\n';
    }
}

void Console::handleScreenCommand(
    const std::string& commandLine
) {
    std::istringstream stream(commandLine);

    std::string command;
    std::string option;
    std::string processName;

    stream >> command >> option;

    if (option == "-ls") {
        std::cout << Report::generate(scheduler);
        return;
    }

    if (option == "-s" || option == "-r") {
        stream >> processName;

        if (processName.empty()) {
            std::cout
                << "Please provide a process name.\n";
            return;
        }

        // Create a new process.
        if (option == "-s") {
            auto process = scheduler.createProcess(processName);

            if (!process) {
                std::cout
                    << "Process name already exists or is invalid.\n";
                return;
            }

            enterProcessScreen(processName, true);
            return;
        }

        // Resume an existing process only if it is not finished.
        auto process = scheduler.findProcess(processName);

        if (!process ||
            process->getState() == ProcessState::Finished) {
            std::cout
                << "Process "
                << processName
                << " not found.\n";
            return;
        }

        enterProcessScreen(processName);
        return;
    }

    std::cout
        << "Usage: screen -s <name>, screen -r <name>, "
        << "or screen -ls\n";
}

void Console::printProcessInfo(const ProcessSnapshot& process) const {
    std::cout << "\nProcess name: " << process.name << '\n'
        << "ID: " << process.id << '\n'
        << "Logs:\n";
    for (const auto& log : process.logs) { std::cout << log << '\n'; }
    std::cout << '\n';
    if (process.state == ProcessState::Finished) {
        std::cout << "Finished!\n\n";
    }
    else {
        std::cout << "Current instruction line: " << process.executed << '\n'
            << "Lines of code: " << process.total << "\n\n";
    }
}

void Console::enterProcessScreen(const std::string& processName, bool newlyCreated) {
    const auto process = scheduler.findProcess(processName);
    if (!process) {
        std::cout << "Process " << processName << " not found.\n";
        return;
    }
    const auto snapshot = process->getSnapshot(true);
    if (!newlyCreated && snapshot.state == ProcessState::Finished) {
        std::cout << "Process " << processName << " not found.\n";
        return;
    }
    clearConsole();
    std::cout << "============================================\n"
              << "Process Screen: " << processName << '\n'
              << "Type process-smi for updated information.\n"
              << "Type exit to return to the main menu.\n"
              << "============================================\n";
    printProcessInfo(snapshot);
    while (true) {
        const std::string line = readLine(processName + "> ");
        if (!std::cin) { break; }
        std::istringstream stream(line);
        std::string command, extra;
        if (!(stream >> command)) { continue; }
        if (stream >> extra) {
            std::cout << "Use process-smi or exit without extra arguments.\n";
        }
        else if (command == "exit") { break; }
        else if (command == "process-smi") { printProcessInfo(process->getSnapshot(true)); }
        else { std::cout << "Unknown process command.\n"; }
    }
    clearConsole();
    printWelcome();
}
