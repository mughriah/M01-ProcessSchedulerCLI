#include "report.h"

#include <fstream>
#include <sstream>
#include <cmath>

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
    out << "\nReady / sleeping processes:\n";
    for (const auto& p : snapshot.processes) {
        if (p.state == ProcessState::Finished || p.coreID >= 0) { continue; }
        out << p.name << "   (" << p.creationTime << ")    " << processStateName(p.state)
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
