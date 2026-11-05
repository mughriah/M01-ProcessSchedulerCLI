#include "config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

bool Config::loadFromFile(const std::string& filename, std::string* error) {
    auto fail = [error](const std::string& message) {
        if (error) { *error = message; }
        return false;
    };
    if (error) { error->clear(); }
    std::ifstream file(filename);
    if (!file) { return fail("Cannot open " + filename + ". Run from the project folder."); }

    constexpr uint64_t maximum = uint64_t{1} << 32;
    const std::set<std::string> required = {
        "num-cpu", "scheduler", "quantum-cycles", "batch-process-freq",
        "min-ins", "max-ins", "delays-per-exec"
    };
    Config temp;
    std::set<std::string> seen;
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        std::istringstream stream(line);
        std::string key, value, extra;
        if (!(stream >> key)) { continue; }
        if (!(stream >> std::quoted(value)) || (stream >> extra)) {
            return fail("Expected one parameter and one value on line " + std::to_string(lineNumber) + ".");
        }
        if (key == "delay-per-exec") { key = "delays-per-exec"; }
        if (!required.count(key)) { return fail("Unknown parameter: " + key + "."); }
        if (!seen.insert(key).second) { return fail("Duplicate parameter: " + key + "."); }
        if (key == "scheduler") {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (value != "fcfs" && value != "rr") {
                return fail("scheduler must be fcfs or rr (quotes are optional).");
            }
            temp.scheduler = value;
            continue;
        }
        if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        })) { return fail(key + " must be a whole nonnegative number."); }
        uint64_t number;
        try { number = std::stoull(value); }
        catch (const std::exception&) { return fail(key + " is outside the allowed range."); }
        const uint64_t minimum = key == "delays-per-exec" ? 0 : 1;
        const uint64_t limit = key == "num-cpu" ? 128 : maximum;
        if (number < minimum || number > limit) {
            return fail(key + " must be between " + std::to_string(minimum) + " and " + std::to_string(limit) + ".");
        }
        if (key == "num-cpu") { temp.numCPU = static_cast<int>(number); }
        else if (key == "quantum-cycles") { temp.quantumCycles = number; }
        else if (key == "batch-process-freq") { temp.batchProcessFreq = number; }
        else if (key == "min-ins") { temp.minInstructions = number; }
        else if (key == "max-ins") { temp.maxInstructions = number; }
        else if (key == "delays-per-exec") { temp.delaysPerExec = number; }
    }
    if (file.bad()) { return fail("Could not read " + filename + "."); }
    for (const auto& key : required) {
        if (!seen.count(key)) { return fail("Missing parameter: " + key + "."); }
    }
    if (temp.minInstructions > temp.maxInstructions) {
        return fail("min-ins must not exceed max-ins.");
    }
    *this = temp;
    return true;
}
