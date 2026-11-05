#pragma once

#include "instruction.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

enum class ProcessState { Ready, Running, Sleeping, Finished };
enum class StepResult { Executed, BusyWait, Sleeping, Finished };
std::string processStateName(ProcessState state);

struct ProcessSnapshot {
    std::string name;
    int id = 0;
    ProcessState state = ProcessState::Ready;
    int coreID = -1;
    uint64_t executed = 0;
    uint64_t total = 0;
    uint64_t creationTick = 0;
    uint64_t finishTick = 0;
    std::string creationTime;
    std::string finishTime;
    std::vector<std::string> logs;
};

class Process {
private:
    struct ExecutionFrame {
        const std::vector<Instruction>* block;
        size_t index;
        uint64_t repetitionsLeft;
    };
    mutable std::mutex mutex;
    std::string name;
    int processID;
    std::vector<Instruction> instructions;
    std::vector<ExecutionFrame> frames;
    std::map<std::string, uint16_t> variables;
    std::vector<std::string> logs;
    ProcessState state = ProcessState::Ready;
    int coreID = -1;
    uint64_t instructionCount = 0;
    uint64_t creationTick = 0;
    uint64_t startTick = 0;
    uint64_t finishTick = 0;
    uint64_t executedInstructions = 0;
    uint64_t sleepRemaining = 0;
    uint64_t delayRemaining = 0;
    std::string creationTime;
    std::string finishTime;

    uint16_t resolveOperand(const std::string& operand);
    std::string expandMessage(const std::string& message);
    const Instruction* nextInstruction();
    void finish(uint64_t currentTick);

public:
    Process(int id, const std::string& processName, std::vector<Instruction> program, uint64_t createdAt);
    StepResult executeOne(uint64_t currentTick, uint64_t delayPerExec);
    void advanceSleep(uint64_t currentTick);
    void setRunning(uint64_t currentTick, int cpu = 0);
    void setReady();
    ProcessSnapshot getSnapshot(bool includeLogs = false) const;
    std::string getName() const;
    int getID() const;
    ProcessState getState() const;
    uint64_t getInstructionPointer() const;
    uint64_t getInstructionCount() const;
    uint64_t getCreationTick() const;
    uint64_t getStartTick() const;
    uint64_t getFinishTick() const;
    uint64_t getExecutedInstructions() const;
    uint64_t getSleepRemaining() const;
    std::vector<std::string> getLogs() const;
    std::string getStateString() const;
};
