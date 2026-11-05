#include "report.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>

//std::string Report::generate(Scheduler& scheduler) {
//    const auto snapshot = scheduler.getSnapshot();
//    const double utilization = snapshot.cpuCount > 0
//        ? 100.0 * snapshot.usedCores / snapshot.cpuCount : 0.0;
//    std::ostringstream output;
//    output << "========================================\n"
//           << "          CPU UTILIZATION REPORT\n"
//           << "========================================\n"
//           << "CPU utilization: " << std::fixed << std::setprecision(2) << utilization << "%\n"
//           << "CPU cores: " << snapshot.cpuCount << '\n'
//           << "Cores used: " << snapshot.usedCores << '\n'
//           << "Cores available: " << snapshot.cpuCount - snapshot.usedCores << '\n'
//           << "Total CPU ticks: " << snapshot.tick << "\n\n"
//           << "Running / Active Processes\n"
//           << "----------------------------------------\n";
//    bool foundActive = false;
//    for (const auto& process : snapshot.processes) {
//        if (process.state == ProcessState::Finished) { continue; }
//        foundActive = true;
//        output << process.name << " | ID: " << process.id
//               << " | Created: " << process.creationTime
//               << " | State: " << processStateName(process.state)
//               << " | Core: " << (process.coreID >= 0 ? std::to_string(process.coreID) : "-")
//               << " | Instructions: " << process.executed << '/' << process.total << '\n';
//    }
//    if (!foundActive) { output << "No active processes.\n"; }
//    output << "\nFinished Processes\n----------------------------------------\n";
//    bool foundFinished = false;
//    for (const auto& process : snapshot.processes) {
//        if (process.state != ProcessState::Finished) { continue; }
//        foundFinished = true;
//        output << process.name << " | ID: " << process.id
//               << " | Finished: " << process.finishTime
//               << " | Finished at tick: " << process.finishTick
//               << " | Instructions: " << process.executed << '/' << process.total << '\n';
//    }
//    if (!foundFinished) { output << "No finished processes.\n"; }
//    output << "\n========================================\n";
//    return output.str();
//}

std::string Report::generate(Scheduler& scheduler) {
    const auto snapshot = scheduler.getSnapshot();
    const int utilization = snapshot.cpuCount > 0
        ? static_cast<int>(std::lround(100.0 * snapshot.usedCores / snapshot.cpuCount)) : 0;
    std::ostringstream out;
    out << "CPU utilization: " << utilization << "%\n"
        << "Cores used: " << snapshot.usedCores << '\n'
        << "Cores available: " << snapshot.cpuCount - snapshot.usedCores << "\n\n"
        << "--------------------------------------\n"
        << "Running processes:\n";
    for (const auto& p : snapshot.processes) {
        if (p.state == ProcessState::Finished || p.coreID < 0) { continue; }
        out << p.name << "   (" << p.creationTime << ")    Core: "
            << (p.coreID >= 0 ? std::to_string(p.coreID) : "-")
            << "     " << p.executed << " / " << p.total << '\n';
    }
    out << "\nFinished processes:\n";
    for (const auto& p : snapshot.processes) {
        if (p.state != ProcessState::Finished) { continue; }
        out << p.name << "   (" << p.creationTime << ")    Finished     "
            << p.executed << " / " << p.total << '\n';
    }
    out << "--------------------------------------\n";
    return out.str();
}

bool Report::save(const std::string& report, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) { return false; }
    file << report;
    file.close();
    return !file.fail();
}
