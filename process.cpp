#include "Process.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

Process::Process(
    int id,
    const std::string& processName,
    std::vector<Instruction> program,
    uint64_t createdAt
)
    : name(processName),
    processID(id),
    instructions(std::move(program)),
    creationTick(createdAt) {
}

std::string Process::evaluateOperand(const std::string& operand) {
    return operand;
}

uint16_t Process::resolveOperand(const std::string& operand) {
    if (operand.empty()) {
        return 0;
    }

    bool numeric = true;

    for (unsigned char c : operand) {
        if (!std::isdigit(c)) {
            numeric = false;
            break;
        }
    }

    if (numeric) {
        try {
            unsigned long long number = std::stoull(operand);

            return static_cast<uint16_t>(
                std::min<unsigned long long>(
                    number,
                    std::numeric_limits<uint16_t>::max()
                )
                );
        }
        catch (...) {
            return std::numeric_limits<uint16_t>::max();
        }
    }

    auto it = variables.find(operand);

    if (it == variables.end()) {
        variables[operand] = 0;
        return 0;
    }

    return it->second;
}

std::string Process::expandMessage(const std::string& message) {
    std::string result;
    size_t i = 0;

    while (i < message.size()) {
        if (message[i] == '+') {
            size_t start = i + 1;
            size_t end = start;

            while (end < message.size() &&
                (std::isalnum(
                    static_cast<unsigned char>(message[end])
                ) || message[end] == '_')) {
                ++end;
            }

            if (end > start) {
                std::string variable = message.substr(
                    start,
                    end - start
                );

                result += std::to_string(
                    resolveOperand(variable)
                );

                i = end;
                continue;
            }
        }

        result += message[i];
        ++i;
    }

    return result;
}

StepResult Process::executeOne(
    uint64_t currentTick,
    uint64_t delayPerExec
) {
    std::lock_guard<std::mutex> lock(mutex);

    if (state == ProcessState::Finished) {
        return StepResult::Finished;
    }

    if (state == ProcessState::Sleeping) {
        return StepResult::Sleeping;
    }

    if (delayRemaining > 0) {
        --delayRemaining;
        return StepResult::BusyWait;
    }

    if (instructionPointer >= instructions.size()) {
        state = ProcessState::Finished;
        finishTick = currentTick;
        return StepResult::Finished;
    }

    const Instruction& instruction = instructions[instructionPointer];

    switch (instruction.type) {
    case InstructionType::PRINT:
        logs.push_back(expandMessage(instruction.message));
        break;

    case InstructionType::DECLARE:
        variables[instruction.destination] = instruction.value;
        break;

    case InstructionType::ADD: {
        uint32_t left = resolveOperand(instruction.operand1);
        uint32_t right = resolveOperand(instruction.operand2);

        uint32_t result = left + right;

        result = std::min<uint32_t>(
            result,
            std::numeric_limits<uint16_t>::max()
        );

        variables[instruction.destination] =
            static_cast<uint16_t>(result);
        break;
    }

    case InstructionType::SUBTRACT: {
        uint32_t left = resolveOperand(instruction.operand1);
        uint32_t right = resolveOperand(instruction.operand2);

        uint32_t result = left >= right ? left - right : 0;

        variables[instruction.destination] =
            static_cast<uint16_t>(result);
        break;
    }

    case InstructionType::SLEEP:
        ++instructionPointer;
        ++executedInstructions;

        sleepRemaining = instruction.sleepTicks;
        state = ProcessState::Sleeping;

        return StepResult::Sleeping;
    }

    ++instructionPointer;
    ++executedInstructions;

    if (delayPerExec > 0) {
        delayRemaining = delayPerExec;
    }

    if (instructionPointer >= instructions.size()) {
        state = ProcessState::Finished;
        finishTick = currentTick;
        return StepResult::Finished;
    }

    return StepResult::Executed;
}

void Process::advanceSleep() {
    std::lock_guard<std::mutex> lock(mutex);

    if (state != ProcessState::Sleeping) {
        return;
    }

    if (sleepRemaining > 0) {
        --sleepRemaining;
    }

    if (sleepRemaining == 0) {
        state = ProcessState::Ready;
    }
}

void Process::setRunning(uint64_t currentTick) {
    std::lock_guard<std::mutex> lock(mutex);

    if (state == ProcessState::Finished) {
        return;
    }

    state = ProcessState::Running;

    if (startTick == 0) {
        startTick = currentTick;
    }
}

void Process::setReady() {
    std::lock_guard<std::mutex> lock(mutex);

    if (state != ProcessState::Finished &&
        state != ProcessState::Sleeping) {
        state = ProcessState::Ready;
    }
}

std::string Process::getName() const {
    std::lock_guard<std::mutex> lock(mutex);
    return name;
}

int Process::getID() const {
    std::lock_guard<std::mutex> lock(mutex);
    return processID;
}

ProcessState Process::getState() const {
    std::lock_guard<std::mutex> lock(mutex);
    return state;
}

size_t Process::getInstructionPointer() const {
    std::lock_guard<std::mutex> lock(mutex);
    return instructionPointer;
}

size_t Process::getInstructionCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return instructions.size();
}

uint64_t Process::getCreationTick() const {
    std::lock_guard<std::mutex> lock(mutex);
    return creationTick;
}

uint64_t Process::getStartTick() const {
    std::lock_guard<std::mutex> lock(mutex);
    return startTick;
}

uint64_t Process::getFinishTick() const {
    std::lock_guard<std::mutex> lock(mutex);
    return finishTick;
}

uint64_t Process::getExecutedInstructions() const {
    std::lock_guard<std::mutex> lock(mutex);
    return executedInstructions;
}

uint64_t Process::getSleepRemaining() const {
    std::lock_guard<std::mutex> lock(mutex);
    return sleepRemaining;
}

std::vector<std::string> Process::getLogs() const {
    std::lock_guard<std::mutex> lock(mutex);
    return logs;
}

std::string Process::getStateString() const {
    std::lock_guard<std::mutex> lock(mutex);

    switch (state) {
    case ProcessState::Ready:
        return "Ready";

    case ProcessState::Running:
        return "Running";

    case ProcessState::Sleeping:
        return "Sleeping";

    case ProcessState::Finished:
        return "Finished";
    }

    return "Unknown";
}