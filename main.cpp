
#include "console.h"

#include <exception>
#include <iostream>

int main() {
    // Display project header
    std::cout
        << "============================================================\n"
        << "       Group 8 | CSOPESY - M01 CPU Scheduler and CLI\n"
        << "============================================================\n"
        << "  Developers:\n"
        << "    Abenojar, Fred\n"
        << "    Caya, Mary Faye\n"
        << "    Diamante, Deo Zamir\n"
        << "    Guiller, Gerylyn\n"
        << "\n"
        << "  Version Date: November 5, 2026\n"
        << "============================================================\n\n";

    // Display available commands
    std::cout
        << "AVAILABLE COMMANDS\n"
        << "------------------------------------------------------------\n"
        << "  initialize                 Initialize the scheduler\n"
        << "  screen -s <name>           Create and open a process\n"
        << "  screen -r <name>           Resume an existing process\n"
        << "  screen -ls                 List processes and CPU status\n"
        << "  scheduler-start            Start automatic process generation\n"
        << "  scheduler-stop             Stop automatic process generation\n"
        << "  report-util                Generate the utilization report\n"
        << "  help                       Display available commands\n"
        << "  exit                       Exit the program\n"
        << "------------------------------------------------------------\n"
        << "  Inside a process screen: process-smi shows that process.\n\n";

    try {
        Console console;
        console.run();
    }
    catch (const std::exception& exception) {
        std::cerr
            << "Fatal error: "
            << exception.what()
            << '\n';

        return 1;
    }

    return 0;
}
