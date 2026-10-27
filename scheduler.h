
#pragma once

#include "Config.h"
#include "Process.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Scheduler {
private:
    struct CPUCore {
        std::shared_ptr<Process> process;
        uint64_t quantumUsed = 0;
    };

    Config config;

    mutable std::mutex mutex;

    std::unordered_map<
        std::string,
        std::shared_ptr<Process>
    > processes;

    std::deque<std::shared_ptr<Process>> readyQueue;
    std::vector<CPUCore> cores;

    std::thread worker;

    std::atomic<bool> shutdownRequested{ false };
    std::atomic<bool> batchGenerationEnabled{ false };

    uint64_t currentTick = 0;
    uint64_t busyCoreTicks = 0;
    uint64_t nextProcessNumber = 1;

    void run();
    void tick();
    void generateBatchProcess();

    void enqueueIfNeeded(
        const std::shared_ptr<Process>& process
    );

public:
    Scheduler();
    ~Scheduler();

    bool initialize(const Config& loadedConfig);

    void start();
    void stop();

    void startBatchGeneration();
    void stopBatchGeneration();

    std::shared_ptr<Process> createProcess(
        const std::string& processName
    );

    std::shared_ptr<Process> findProcess(
        const std::string& processName
    ) const;

    std::vector<std::shared_ptr<Process>>
        getAllProcesses() const;

    uint64_t getCurrentTick() const;
    uint64_t getBusyCoreTicks() const;

    int getNumCPU() const;
    int getAvailableCores() const;

    Config getConfig() const;
};
