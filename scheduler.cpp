
#include "Scheduler.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

Scheduler::Scheduler() {
}

Scheduler::~Scheduler() {
    shutdownRequested = true;
    batchGenerationEnabled = false;

    if (worker.joinable()) {
        worker.join();
    }
}

bool Scheduler::initialize(const Config& loadedConfig) {
    std::lock_guard<std::mutex> lock(mutex);

    if (worker.joinable()) {
        return false;
    }

    config = loadedConfig;
    cores.clear();
    cores.resize(static_cast<size_t>(config.numCPU));

    currentTick = 0;
    busyCoreTicks = 0;
    nextProcessNumber = 1;

    processes.clear();
    readyQueue.clear();

    shutdownRequested = false;
    batchGenerationEnabled = false;

    return true;
}

void Scheduler::start() {
    if (worker.joinable()) {
        return;
    }

    shutdownRequested = false;
    worker = std::thread(&Scheduler::run, this);
}

void Scheduler::stop() {
    shutdownRequested = true;
    batchGenerationEnabled = false;

    if (worker.joinable()) {
        worker.join();
    }
}

void Scheduler::startBatchGeneration() {
    batchGenerationEnabled = true;
}

void Scheduler::stopBatchGeneration() {
    batchGenerationEnabled = false;
}

void Scheduler::enqueueIfNeeded(
    const std::shared_ptr<Process>& process
) {
    if (!process ||
        process->getState() != ProcessState::Ready) {
        return;
    }

    for (const auto& queued : readyQueue) {
        if (queued == process) {
            return;
        }
    }

    for (const auto& core : cores) {
        if (core.process == process) {
            return;
        }
    }

    readyQueue.push_back(process);
}

std::shared_ptr<Process> Scheduler::createProcess(
    const std::string& processName
) {
    std::lock_guard<std::mutex> lock(mutex);

    if (processName.empty() ||
        processes.find(processName) != processes.end()) {
        return nullptr;
    }

    std::uniform_int_distribution<uint64_t> countDistribution(
        config.minInstructions,
        config.maxInstructions
    );

    static thread_local std::mt19937 rng(
        std::random_device{}()
    );

    uint64_t count = countDistribution(rng);

    auto program = generateInstructions(processName, count);

    int id = static_cast<int>(processes.size() + 1);

    auto process = std::make_shared<Process>(
        id,
        processName,
        std::move(program),
        currentTick
    );

    processes[processName] = process;
    readyQueue.push_back(process);

    return process;
}

std::shared_ptr<Process> Scheduler::findProcess(
    const std::string& processName
) const {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = processes.find(processName);

    if (it == processes.end()) {
        return nullptr;
    }

    // Return active and finished processes.
    // This allows process-smi to display final information.
    return it->second;
}

std::vector<std::shared_ptr<Process>>
Scheduler::getAllProcesses() const {
    std::lock_guard<std::mutex> lock(mutex);

    std::vector<std::shared_ptr<Process>> result;
    result.reserve(processes.size());

    for (const auto& entry : processes) {
        result.push_back(entry.second);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a, const auto& b) {
            return a->getID() < b->getID();
        }
    );

    return result;
}

uint64_t Scheduler::getCurrentTick() const {
    std::lock_guard<std::mutex> lock(mutex);
    return currentTick;
}

uint64_t Scheduler::getBusyCoreTicks() const {
    std::lock_guard<std::mutex> lock(mutex);
    return busyCoreTicks;
}

int Scheduler::getNumCPU() const {
    std::lock_guard<std::mutex> lock(mutex);
    return config.numCPU;
}

int Scheduler::getAvailableCores() const {
    std::lock_guard<std::mutex> lock(mutex);

    int available = 0;

    for (const auto& core : cores) {
        if (!core.process) {
            ++available;
        }
    }

    return available;
}

Config Scheduler::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex);
    return config;
}

void Scheduler::generateBatchProcess() {
    std::ostringstream name;

    name << 'p'
        << std::setw(2)
        << std::setfill('0')
        << nextProcessNumber++;

    while (processes.find(name.str()) != processes.end()) {
        name.str("");
        name.clear();

        name << 'p'
            << std::setw(2)
            << std::setfill('0')
            << nextProcessNumber++;
    }

    std::uniform_int_distribution<uint64_t> countDistribution(
        config.minInstructions,
        config.maxInstructions
    );

    static thread_local std::mt19937 rng(
        std::random_device{}()
    );

    uint64_t count = countDistribution(rng);

    auto program = generateInstructions(name.str(), count);

    int id = static_cast<int>(processes.size() + 1);

    auto process = std::make_shared<Process>(
        id,
        name.str(),
        std::move(program),
        currentTick
    );

    processes[name.str()] = process;
    readyQueue.push_back(process);
}

void Scheduler::tick() {
    std::lock_guard<std::mutex> lock(mutex);

    ++currentTick;

    // Advance sleeping processes and requeue those that wake.
    for (auto& entry : processes) {
        auto& process = entry.second;

        if (process->getState() == ProcessState::Sleeping) {
            process->advanceSleep();
        }

        enqueueIfNeeded(process);
    }

    // Assign ready processes to idle CPU cores.
    for (auto& core : cores) {
        if (core.process) {
            continue;
        }

        while (!readyQueue.empty()) {
            auto process = readyQueue.front();
            readyQueue.pop_front();

            if (process->getState() != ProcessState::Ready) {
                continue;
            }

            core.process = process;
            core.quantumUsed = 0;

            process->setRunning(currentTick);
            break;
        }
    }

    // Execute one CPU tick on every occupied core.
    for (auto& core : cores) {
        if (!core.process) {
            continue;
        }

        auto process = core.process;

        StepResult result = process->executeOne(
            currentTick,
            config.delaysPerExec
        );

        ++busyCoreTicks;
        ++core.quantumUsed;

        if (result == StepResult::Finished ||
            result == StepResult::Sleeping) {
            core.process.reset();
            core.quantumUsed = 0;
            continue;
        }

        if (config.scheduler == "rr" &&
            core.quantumUsed >= config.quantumCycles) {
            process->setReady();

            core.process.reset();
            core.quantumUsed = 0;

            readyQueue.push_back(process);
        }
    }

    // Generate a process at the configured CPU-tick frequency.
    if (batchGenerationEnabled &&
        config.batchProcessFreq > 0 &&
        currentTick % config.batchProcessFreq == 0) {
        generateBatchProcess();
    }
}

void Scheduler::run() {
    using clock = std::chrono::steady_clock;

    auto nextTick = clock::now();

    while (!shutdownRequested) {
        nextTick += std::chrono::milliseconds(100);

        tick();

        std::this_thread::sleep_until(nextTick);
    }
}
