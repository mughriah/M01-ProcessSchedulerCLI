
#include "Console.h"
#include "Report.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

void Console::printWelcome() const {
    std::cout
        << "Type initialize first (or exit).\n\n";
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
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    if (console == INVALID_HANDLE_VALUE) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;

    if (!GetConsoleScreenBufferInfo(console, &info)) {
        return;
    }

    DWORD cells = static_cast<DWORD>(
        info.dwSize.X * info.dwSize.Y
        );

    DWORD written = 0;
    COORD origin = { 0, 0 };

    FillConsoleOutputCharacterA(
        console, ' ', cells, origin, &written
    );

    FillConsoleOutputAttribute(
        console, info.wAttributes, cells, origin, &written
    );

    SetConsoleCursorPosition(console, origin);
#else
    std::cout << "\033[2J\033[H";
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

        if (!config.loadFromFile("config.txt")) {
            std::cout
                << "Failed to load config.txt. "
                << "Check its values and format.\n";
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
        std::cout << Report::generate(scheduler);

        if (Report::save(scheduler)) {
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

            enterProcessScreen(processName);
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

void Console::enterProcessScreen(
    const std::string& processName
) {
    auto process = scheduler.findProcess(processName);

    if (!process) {
        std::cout
            << "Process "
            << processName
            << " not found.\n";
        return;
    }

    // Do not clear the console when entering a process screen.
    std::cout
        << "\n============================================\n"
        << "Process Screen: "
        << processName
        << '\n'
        << "Type process-smi for process information.\n"
        << "Type exit to return to the main menu.\n"
        << "============================================\n";

    while (true) {
        std::string command = readLine(
            processName + "> "
        );

        if (!std::cin || command == "exit") {
            break;
        }

        if (command == "process-smi") {
            process = scheduler.findProcess(processName);

            if (!process) {
                std::cout << "Finished!\n";
                continue;
            }

            std::cout
                << "Process name: "
                << process->getName()
                << '\n'
                << "Process ID: "
                << process->getID()
                << '\n'
                << "State: "
                << process->getStateString()
                << '\n'
                << "Current instruction: "
                << process->getInstructionPointer()
                << '\n'
                << "Total instructions: "
                << process->getInstructionCount()
                << '\n';

            std::cout << "\nProcess output:\n";

            for (const auto& log : process->getLogs()) {
                std::cout << log << '\n';
            }

            if (process->getState() == ProcessState::Finished) {
                std::cout << "Finished!\n";
            }
        }
        else {
            std::cout
                << "Unknown process command.\n";
        }
    }

    // Preserve process output when returning to the main menu.
    std::cout
        << "\nReturning to the main menu...\n\n";
}
