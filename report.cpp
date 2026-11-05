#include "report.h"

#include <fstream>
#include <iomanip>
#include <sstream>

std::string Report::generate(Scheduler& scheduler) {
    const auto snapshot = scheduler.getSnapshot();
    const double utilization = snapshot.cpuCount > 0
        ? 100.0 * snapshot.usedCores / snapshot.cpuCount : 0.0;
    std::ostringstream output;
    output << "========================================\n"
           << "          CPU UTILIZATION REPORT\n"
           << "========================================\n"
           << "CPU utilization: " << std::fixed << std::setprecision(2) << utilization << "%\n"
           << "CPU cores: " << snapshot.cpuCount << '\n'
           << "Cores used: " << snapshot.usedCores << '\n'
           << "Cores available: " << snapshot.cpuCount - snapshot.usedCores << '\n'
           << "Total CPU ticks: " << snapshot.tick << "\n\n"
           << "Running / Active Processes\n"
           << "----------------------------------------\n";
    bool foundActive = false;
    for (const auto& process : snapshot.processes) {
        if (process.state == ProcessState::Finished) { continue; }
        foundActive = true;
        output << process.name << " | ID: " << process.id
               << " | Created: " << process.creationTime
               << " | State: " << processStateName(process.state)
               << " | Core: " << (process.coreID >= 0 ? std::to_string(process.coreID) : "-")
               << " | Instructions: " << process.executed << '/' << process.total << '\n';
    }
    if (!foundActive) { output << "No active processes.\n"; }
    output << "\nFinished Processes\n----------------------------------------\n";
    bool foundFinished = false;
    for (const auto& process : snapshot.processes) {
        if (process.state != ProcessState::Finished) { continue; }
        foundFinished = true;
        output << process.name << " | ID: " << process.id
               << " | Finished: " << process.finishTime
               << " | Finished at tick: " << process.finishTick
               << " | Instructions: " << process.executed << '/' << process.total << '\n';
    }
    if (!foundFinished) { output << "No finished processes.\n"; }
    output << "\n========================================\n";
    return output.str();
}

bool Report::save(const std::string& report, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) { return false; }
    file << report;
    file.close();
    return !file.fail();
}
