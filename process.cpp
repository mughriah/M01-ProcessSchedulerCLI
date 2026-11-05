#include "process.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace {
std::string timestamp() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream text;
    text << std::put_time(&local, "%m/%d/%Y %I:%M:%S%p");
    return text.str();
}
}

std::string processStateName(ProcessState state) {
    switch (state) {
    case ProcessState::Ready: return "Ready";
    case ProcessState::Running: return "Running";
    case ProcessState::Sleeping: return "Sleeping";
    case ProcessState::Finished: return "Finished";
    }
    return "Unknown";
}

Process::Process(int id, const std::string& processName, std::vector<Instruction> program, uint64_t createdAt)
    : name(processName), processID(id), instructions(std::move(program)),
      instructionCount(countExecutableInstructions(instructions)), creationTick(createdAt), creationTime(timestamp()) {
    frames.push_back({&instructions, 0, 1});
}

uint16_t Process::resolveOperand(const std::string& operand) {
    if (operand.empty()) { return 0; }
    if (std::all_of(operand.begin(), operand.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
        try {
            return static_cast<uint16_t>(std::min<unsigned long long>(std::stoull(operand), 65535));
        }
        catch (const std::exception&) { return 65535; }
    }
    // Reading an undeclared variable creates it with the required initial value of zero.
    return variables[operand];
}

std::string Process::expandMessage(const std::string& message) {
    std::string result;
    for (size_t i = 0; i < message.size();) {
        if (message[i] == '+') {
            size_t start = i + 1;
            while (start < message.size() && std::isspace(static_cast<unsigned char>(message[start]))) { ++start; }
            size_t end = start;
            while (end < message.size() && (std::isalnum(static_cast<unsigned char>(message[end])) || message[end] == '_')) { ++end; }
            if (end > start) {
                result += std::to_string(resolveOperand(message.substr(start, end - start)));
                i = end;
                continue;
            }
        }
        result += message[i++];
    }
    return result;
}

const Instruction* Process::nextInstruction() {
    // The stack remembers each loop's position across quantum expiry and SLEEP.
    while (!frames.empty()) {
        auto& frame = frames.back();
        if (frame.index == frame.block->size()) {
            if (--frame.repetitionsLeft > 0) { frame.index = 0; }
            else { frames.pop_back(); }
            continue;
        }
        const Instruction* instruction = &(*frame.block)[frame.index++];
        if (instruction->type == InstructionType::FOR) {
            if (instruction->repeats && countExecutableInstructions(instruction->body) > 0) {
                frames.push_back({&instruction->body, 0, instruction->repeats});
            }
            continue;
        }
        return instruction;
    }
    return nullptr;
}

void Process::finish(uint64_t currentTick) {
    state = ProcessState::Finished;
    coreID = -1;
    finishTick = currentTick;
    finishTime = timestamp();
    variables.clear();
    frames.clear();
    instructions.clear();
}

StepResult Process::executeOne(uint64_t currentTick, uint64_t delayPerExec) {
    std::lock_guard<std::mutex> lock(mutex);
    if (state == ProcessState::Finished) { return StepResult::Finished; }
    if (state == ProcessState::Sleeping) { return StepResult::Sleeping; }
    if (delayRemaining > 0) {
        --delayRemaining;
        return StepResult::BusyWait;
    }
    const Instruction* instruction = nextInstruction();
    if (!instruction) {
        finish(currentTick);
        return StepResult::Finished;
    }
    switch (instruction->type) {
    case InstructionType::PRINT:
        logs.push_back("(" + timestamp() + ") Core:" + std::to_string(coreID) + " " + expandMessage(instruction->message));
        break;
    case InstructionType::DECLARE:
        variables[instruction->destination] = instruction->value;
        break;
    case InstructionType::ADD: {
        const uint32_t left = resolveOperand(instruction->operand1);
        const uint32_t right = resolveOperand(instruction->operand2);
        variables[instruction->destination] = static_cast<uint16_t>(std::min<uint32_t>(left + right, 65535));
        break;
    }
    case InstructionType::SUBTRACT: {
        const uint32_t left = resolveOperand(instruction->operand1);
        const uint32_t right = resolveOperand(instruction->operand2);
        variables[instruction->destination] = static_cast<uint16_t>(left >= right ? left - right : 0);
        break;
    }
    case InstructionType::SLEEP:
        sleepRemaining = instruction->sleepTicks;
        break;
    case InstructionType::FOR:
        break; // Loop headers are handled by nextInstruction().
    }
    ++executedInstructions;
    delayRemaining = delayPerExec;
    if (sleepRemaining > 0) {
        state = ProcessState::Sleeping;
        coreID = -1;
        return StepResult::Sleeping;
    }
    if (executedInstructions == instructionCount) {
        finish(currentTick);
        return StepResult::Finished;
    }
    return StepResult::Executed;
}

void Process::advanceSleep(uint64_t currentTick) {
    std::lock_guard<std::mutex> lock(mutex);
    if (state != ProcessState::Sleeping) { return; }
    if (sleepRemaining > 0) { --sleepRemaining; }
    if (sleepRemaining == 0) {
        if (executedInstructions == instructionCount) { finish(currentTick); }
        else { state = ProcessState::Ready; }
    }
}

void Process::setRunning(uint64_t currentTick, int cpu) {
    std::lock_guard<std::mutex> lock(mutex);
    if (state == ProcessState::Finished || state == ProcessState::Sleeping) { return; }
    state = ProcessState::Running;
    coreID = cpu;
    if (startTick == 0) { startTick = currentTick; }
}

void Process::setReady() {
    std::lock_guard<std::mutex> lock(mutex);
    if (state == ProcessState::Running) {
        state = ProcessState::Ready;
        coreID = -1;
    }
}

ProcessSnapshot Process::getSnapshot(bool includeLogs) const {
    std::lock_guard<std::mutex> lock(mutex);
    ProcessSnapshot snapshot;
    snapshot.name = name;
    snapshot.id = processID;
    snapshot.state = state;
    snapshot.coreID = coreID;
    snapshot.executed = executedInstructions;
    snapshot.total = instructionCount;
    snapshot.creationTick = creationTick;
    snapshot.finishTick = finishTick;
    snapshot.creationTime = creationTime;
    snapshot.finishTime = finishTime;
    if (includeLogs) { snapshot.logs = logs; }
    return snapshot;
}

std::string Process::getName() const { std::lock_guard<std::mutex> lock(mutex); return name; }
int Process::getID() const { std::lock_guard<std::mutex> lock(mutex); return processID; }
ProcessState Process::getState() const { std::lock_guard<std::mutex> lock(mutex); return state; }
uint64_t Process::getInstructionPointer() const { std::lock_guard<std::mutex> lock(mutex); return executedInstructions; }
uint64_t Process::getInstructionCount() const { std::lock_guard<std::mutex> lock(mutex); return instructionCount; }
uint64_t Process::getCreationTick() const { std::lock_guard<std::mutex> lock(mutex); return creationTick; }
uint64_t Process::getStartTick() const { std::lock_guard<std::mutex> lock(mutex); return startTick; }
uint64_t Process::getFinishTick() const { std::lock_guard<std::mutex> lock(mutex); return finishTick; }
uint64_t Process::getExecutedInstructions() const { std::lock_guard<std::mutex> lock(mutex); return executedInstructions; }
uint64_t Process::getSleepRemaining() const { std::lock_guard<std::mutex> lock(mutex); return sleepRemaining; }
std::vector<std::string> Process::getLogs() const { std::lock_guard<std::mutex> lock(mutex); return logs; }
std::string Process::getStateString() const { std::lock_guard<std::mutex> lock(mutex); return processStateName(state); }
