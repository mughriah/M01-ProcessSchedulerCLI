
#include "scheduler.h"

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
    nextBatchTick = 0;

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
    std::lock_guard<std::mutex> lock(mutex);
    if (batchGenerationEnabled) { return; }
    nextBatchTick = currentTick + config.batchProcessFreq;
    batchGenerationEnabled = true;
}

void Scheduler::stopBatchGeneration() {
    std::lock_guard<std::mutex> lock(mutex);
    batchGenerationEnabled = false;
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

SchedulerSnapshot Scheduler::getSnapshot() const {
    // Keep the scheduler locked while copying every process, so one report
    // cannot show the same process in both the active and finished sections.
    std::lock_guard<std::mutex> lock(mutex);
    SchedulerSnapshot snapshot;
    snapshot.tick = currentTick;
    snapshot.busyCoreTicks = busyCoreTicks;
    snapshot.cpuCount = config.numCPU;
    for (const auto& core : cores) {
        if (core.process) { ++snapshot.usedCores; }
    }
    for (const auto& entry : processes) {
        snapshot.processes.push_back(entry.second->getSnapshot());
    }
    std::sort(snapshot.processes.begin(), snapshot.processes.end(), [](const auto& a, const auto& b) {
        return a.id < b.id;
    });
    return snapshot;
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

    // An expired quantum yields before the next tick's instruction. Keeping
    // the core assigned until then also makes screen -ls show current usage.
    for (auto& core : cores) {
        if (core.process && config.scheduler == "rr" && core.quantumUsed >= config.quantumCycles) {
            core.process->setReady();
            readyQueue.push_back(core.process);
            core.process.reset();
            core.quantumUsed = 0;
        }
    }

    // Advance sleeping processes and requeue those that wake.
    for (auto& entry : processes) {
        auto& process = entry.second;

        if (process->getState() == ProcessState::Sleeping) {
            process->advanceSleep(currentTick);
            if (process->getState() == ProcessState::Ready) {
                readyQueue.push_back(process);
            }
        }
    }

    // Assign ready processes to idle CPU cores.
    for (size_t coreIndex = 0; coreIndex < cores.size(); ++coreIndex) {
        auto& core = cores[coreIndex];
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

            process->setRunning(currentTick, static_cast<int>(coreIndex));
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

    }

    // Generate a process at the configured CPU-tick frequency.
    if (batchGenerationEnabled &&
        config.batchProcessFreq > 0 &&
        currentTick >= nextBatchTick) {
        generateBatchProcess();
        nextBatchTick = currentTick + config.batchProcessFreq;
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
