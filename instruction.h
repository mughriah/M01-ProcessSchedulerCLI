#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class InstructionType {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR
};

struct Instruction {
    InstructionType type = InstructionType::PRINT;

    std::string message;
    std::string destination;
    std::string operand1;
    std::string operand2;

    uint16_t value = 0;
    uint8_t sleepTicks = 0;
    std::vector<Instruction> body;
    uint64_t repeats = 0;

    static Instruction makePrint(const std::string& message);

    static Instruction makeDeclare(
        const std::string& variable,
        uint16_t value
    );

    static Instruction makeAdd(
        const std::string& destination,
        const std::string& operand1,
        const std::string& operand2
    );

    static Instruction makeSubtract(
        const std::string& destination,
        const std::string& operand1,
        const std::string& operand2
    );

    static Instruction makeSleep(uint8_t ticks);
    static Instruction makeFor(std::vector<Instruction> body, uint64_t repeats);
};

// Loop headers do not consume CPU ticks; count the ordinary instructions they execute.
// Also validates the three-level nesting limit and rejects count overflow.
uint64_t countExecutableInstructions(const std::vector<Instruction>& program, unsigned depth = 0);

std::vector<Instruction> generateInstructions(
    const std::string& processName,
    uint64_t count
);
