#include "Report.h"

#include <fstream>
#include <iomanip>
#include <sstream>

std::string Report::generate(Scheduler& scheduler) {
    std::ostringstream output;

    auto processes = scheduler.getAllProcesses();

    const uint64_t ticks = scheduler.getCurrentTick();
    const uint64_t busyTicks = scheduler.getBusyCoreTicks();
    const int cpuCount = scheduler.getNumCPU();
    const int available = scheduler.getAvailableCores();

    double utilization = 0.0;

    if (ticks > 0 && cpuCount > 0) {
        utilization =
            100.0 * static_cast<double>(busyTicks) /
            (static_cast<double>(ticks) * cpuCount);
    }

    if (utilization > 100.0) {
        utilization = 100.0;
    }

    output << "========================================\n";
    output << "          CPU UTILIZATION REPORT\n";
    output << "========================================\n";

    output << "CPU utilization: "
        << std::fixed << std::setprecision(2)
        << utilization << "%\n";

    output << "CPU cores: " << cpuCount << '\n';
    output << "Cores used: " << cpuCount - available << '\n';
    output << "Cores available: " << available << '\n';
    output << "Total CPU ticks: " << ticks << '\n';
    output << "Busy core ticks: " << busyTicks << "\n\n";

    output << "Running / Active Processes\n";
    output << "----------------------------------------\n";

    bool foundActive = false;

    for (const auto& process : processes) {
        if (process->getState() != ProcessState::Finished) {
            foundActive = true;

            output << process->getName()
                << " | ID: " << process->getID()
                << " | State: " << process->getStateString()
                << " | Instructions: "
                << process->getInstructionPointer()
                << "/" << process->getInstructionCount()
                << '\n';
        }
    }

    if (!foundActive) {
        output << "No active processes.\n";
    }

    output << "\nFinished Processes\n";
    output << "----------------------------------------\n";

    bool foundFinished = false;

    for (const auto& process : processes) {
        if (process->getState() == ProcessState::Finished) {
            foundFinished = true;

            output << process->getName()
                << " | ID: " << process->getID()
                << " | Finished at tick: "
                << process->getFinishTick()
                << " | Instructions executed: "
                << process->getExecutedInstructions()
                << '\n';
        }
    }

    if (!foundFinished) {
        output << "No finished processes.\n";
    }

    output << "\n========================================\n";

    return output.str();
}

bool Report::save(
    Scheduler& scheduler,
    const std::string& filename
) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    file << generate(scheduler);

    return file.good();
}