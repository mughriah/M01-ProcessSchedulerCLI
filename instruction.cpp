#include "instruction.h"

#include <algorithm>
#include <random>
#include <limits>
#include <stdexcept>
#include <utility>

Instruction Instruction::makePrint(const std::string& msg) {
    Instruction instruction;
    instruction.type = InstructionType::PRINT;
    instruction.message = msg;
    return instruction;
}

Instruction Instruction::makeDeclare(
    const std::string& variable,
    uint16_t initialValue
) {
    Instruction instruction;
    instruction.type = InstructionType::DECLARE;
    instruction.destination = variable;
    instruction.value = initialValue;
    return instruction;
}

Instruction Instruction::makeAdd(
    const std::string& destination,
    const std::string& operand1,
    const std::string& operand2
) {
    Instruction instruction;
    instruction.type = InstructionType::ADD;
    instruction.destination = destination;
    instruction.operand1 = operand1;
    instruction.operand2 = operand2;
    return instruction;
}

Instruction Instruction::makeSubtract(
    const std::string& destination,
    const std::string& operand1,
    const std::string& operand2
) {
    Instruction instruction;
    instruction.type = InstructionType::SUBTRACT;
    instruction.destination = destination;
    instruction.operand1 = operand1;
    instruction.operand2 = operand2;
    return instruction;
}

Instruction Instruction::makeSleep(uint8_t ticks) {
    Instruction instruction;
    instruction.type = InstructionType::SLEEP;
    instruction.sleepTicks = ticks;
    return instruction;
}

Instruction Instruction::makeFor(std::vector<Instruction> body, uint64_t repeats) {
    Instruction instruction;
    instruction.type = InstructionType::FOR;
    instruction.body = std::move(body);
    instruction.repeats = repeats;
    return instruction;
}

uint64_t countExecutableInstructions(const std::vector<Instruction>& program, unsigned depth) {
    uint64_t total = 0;
    for (const auto& instruction : program) {
        uint64_t count = 1;
        if (instruction.type == InstructionType::FOR) {
            if (depth >= 3) { throw std::invalid_argument("FOR loops may nest at most three levels."); }
            const uint64_t bodyCount = countExecutableInstructions(instruction.body, depth + 1);
            if (instruction.repeats && bodyCount > std::numeric_limits<uint64_t>::max() / instruction.repeats) {
                throw std::overflow_error("FOR instruction count overflow.");
            }
            count = bodyCount * instruction.repeats;
        }
        if (count > std::numeric_limits<uint64_t>::max() - total) {
            throw std::overflow_error("Program instruction count overflow.");
        }
        total += count;
    }
    return total;
}

namespace {
std::vector<Instruction> generateBlock(
    const std::string& processName,
    uint64_t count,
    unsigned depth
) {
    std::vector<Instruction> instructions;

    static thread_local std::mt19937 rng(
        std::random_device{}()
    );

    std::uniform_int_distribution<int> operation(0, 5);
    std::uniform_int_distribution<int> number(0, 100);
    std::uniform_int_distribution<int> sleepLength(1, 5);

    const std::string variable = "var";

    uint64_t remaining = count;
    while (remaining > 0) {
        int choice = operation(rng);
        // A large workload stays compact instead of allocating billions of instructions.
        if (remaining > 4096 && depth < 3) { choice = 5; }
        if (choice == 5 && remaining >= 2 && depth < 3) {
            const uint64_t bodyCount = std::uniform_int_distribution<uint64_t>(1, std::min<uint64_t>(8, remaining / 2))(rng);
            const uint64_t repeats = remaining > 4096
                ? remaining / bodyCount
                : std::uniform_int_distribution<uint64_t>(2, std::min<uint64_t>(4, remaining / bodyCount))(rng);
            instructions.push_back(Instruction::makeFor(generateBlock(processName, bodyCount, depth + 1), repeats));
            remaining -= bodyCount * repeats;
            continue;
        }
        if (choice == 5) { choice = 0; }
        switch (choice) {
        case 0:
            instructions.push_back(
                Instruction::makePrint(
                    "Hello world from " + processName + "!"
                )
            );
            break;

        case 1:
            instructions.push_back(
                Instruction::makeDeclare(
                    variable,
                    static_cast<uint16_t>(number(rng))
                )
            );
            break;

        case 2:
            instructions.push_back(
                Instruction::makeAdd(
                    variable,
                    variable,
                    std::to_string(number(rng))
                )
            );
            break;

        case 3:
            instructions.push_back(
                Instruction::makeSubtract(
                    variable,
                    variable,
                    std::to_string(number(rng))
                )
            );
            break;

        case 4:
            instructions.push_back(
                Instruction::makeSleep(
                    static_cast<uint8_t>(sleepLength(rng))
                )
            );
            break;
        }
        --remaining;
    }

    return instructions;
}
}

std::vector<Instruction> generateInstructions(const std::string& processName, uint64_t count) {
    return generateBlock(processName, count, 0);
}
