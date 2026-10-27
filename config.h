
#pragma once
#include <cstdint>
#include <string>

struct Config {
    int numCPU = 4;
    std::string scheduler = "rr";
    uint64_t quantumCycles = 5;
    uint64_t batchProcessFreq = 10;
    uint64_t minInstructions = 5;
    uint64_t maxInstructions = 15;
    uint64_t delaysPerExec = 0;

    bool loadFromFile(const std::string& filename);
};