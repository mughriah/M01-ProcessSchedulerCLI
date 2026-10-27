#include "Config.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    Config temp = *this;
    std::string key;
    std::string value;

    try {
        while (file >> key >> value) {
            if (key == "num-cpu") {
                temp.numCPU = std::stoi(value);
            }
            else if (key == "scheduler") {
                temp.scheduler = value;
                std::transform(
                    temp.scheduler.begin(),
                    temp.scheduler.end(),
                    temp.scheduler.begin(),
                    [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    }
                );
            }
            else if (key == "quantum-cycles") {
                temp.quantumCycles = std::stoull(value);
            }
            else if (key == "batch-process-freq") {
                temp.batchProcessFreq = std::stoull(value);
            }
            else if (key == "min-ins") {
                temp.minInstructions = std::stoull(value);
            }
            else if (key == "max-ins") {
                temp.maxInstructions = std::stoull(value);
            }
            else if (key == "delays-per-exec") {
                temp.delaysPerExec = std::stoull(value);
            }
        }
    }
    catch (const std::exception&) {
        return false;
    }

    if (temp.numCPU < 1 || temp.numCPU > 128)
        return false;

    if (temp.scheduler != "fcfs" && temp.scheduler != "rr")
        return false;

    if (temp.quantumCycles < 1)
        return false;

    if (temp.batchProcessFreq < 1)
        return false;

    if (temp.minInstructions < 1 ||
        temp.maxInstructions < temp.minInstructions)
        return false;

    temp.numCPU = std::clamp(temp.numCPU, 1, 128);

    *this = temp;
    return true;
}