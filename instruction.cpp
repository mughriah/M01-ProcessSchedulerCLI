#include "Instruction.h"

#include <algorithm>
#include <random>

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

std::vector<Instruction> generateInstructions(
    const std::string& processName,
    uint64_t count
) {
    std::vector<Instruction> instructions;
    instructions.reserve(static_cast<size_t>(count));

    static thread_local std::mt19937 rng(
        std::random_device{}()
    );

    std::uniform_int_distribution<int> operation(0, 4);
    std::uniform_int_distribution<int> number(0, 100);
    std::uniform_int_distribution<int> sleepLength(1, 5);

    const std::string variable = "var";

    for (uint64_t i = 0; i < count; ++i) {
        switch (operation(rng)) {
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
    }

    return instructions;
}