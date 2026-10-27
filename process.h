#pragma once
#pragma once

#include "Instruction.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

enum class ProcessState {
    Ready,
    Running,
    Sleeping,
    Finished
};

enum class StepResult {
    Executed,
    BusyWait,
    Sleeping,
    Finished
};

class Process {
private:
    mutable std::mutex mutex;

    std::string name;
    int processID;

    std::vector<Instruction> instructions;
    std::map<std::string, uint16_t> variables;
    std::vector<std::string> logs;

    size_t instructionPointer = 0;
    ProcessState state = ProcessState::Ready;

    uint64_t creationTick = 0;
    uint64_t startTick = 0;
    uint64_t finishTick = 0;
    uint64_t executedInstructions = 0;
    uint64_t sleepRemaining = 0;
    uint64_t delayRemaining = 0;

    std::string evaluateOperand(const std::string& operand);
    uint16_t resolveOperand(const std::string& operand);
    std::string expandMessage(const std::string& message);

public:
    Process(
        int id,
        const std::string& processName,
        std::vector<Instruction> program,
        uint64_t createdAt
    );

    StepResult executeOne(uint64_t currentTick, uint64_t delayPerExec);

    void advanceSleep();

    void setRunning(uint64_t currentTick);
    void setReady();

    std::string getName() const;
    int getID() const;
    ProcessState getState() const;

    size_t getInstructionPointer() const;
    size_t getInstructionCount() const;

    uint64_t getCreationTick() const;
    uint64_t getStartTick() const;
    uint64_t getFinishTick() const;
    uint64_t getExecutedInstructions() const;

    uint64_t getSleepRemaining() const;

    std::vector<std::string> getLogs() const;

    std::string getStateString() const;
};