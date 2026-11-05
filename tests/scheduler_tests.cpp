#include "../config.h"
#include "../instruction.h"
#include "../process.h"
#include "../report.h"
#include "../scheduler.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

// Advance exact ticks without wall-clock sleeps or adding test commands to the CLI.
struct SchedulerTestAccess {
    static void tick(Scheduler& scheduler) { scheduler.tick(); }
    static std::shared_ptr<Process> add(Scheduler& scheduler, const std::string& name, std::vector<Instruction> program) {
        auto process = std::make_shared<Process>(static_cast<int>(scheduler.processes.size() + 1), name, std::move(program), scheduler.currentTick);
        scheduler.processes[name] = process;
        scheduler.readyQueue.push_back(process);
        return process;
    }
};

namespace {
int checks = 0;
void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) { throw std::runtime_error(message); }
}
const std::string sample = "num-cpu 4\nscheduler \"rr\"\nquantum-cycles 2\nbatch-process-freq 2\nmin-ins 3\nmax-ins 5\ndelays-per-exec 0\n";
std::string setting(std::string text, const std::string& key, const std::string& value) {
    const auto start = text.find(key + " ");
    const auto end = text.find('\n', start);
    text.replace(start, end - start, key + " " + value);
    return text;
}
bool load(Config& config, const std::string& text, std::string* error = nullptr) {
    const auto path = std::filesystem::path("build") / "test-data" / "config.txt";
    { std::ofstream file(path); file << text; }
    return config.loadFromFile(path.string(), error);
}
std::vector<Instruction> prints(int count, const std::string& text = "hello") {
    return std::vector<Instruction>(count, Instruction::makePrint(text));
}
void configTests() {
    Config c;
    std::string error;
    check(load(c, sample, &error) && error.empty(), "spec sample with quoted rr");
    check(load(c, setting(sample, "scheduler", "fcfs")) && c.scheduler == "fcfs", "unquoted fcfs");
    check(load(c, setting(sample, "scheduler", "\"FCFS\"")), "case insensitive quoted scheduler");
    for (const auto& key : {"num-cpu", "scheduler", "quantum-cycles", "batch-process-freq", "min-ins", "max-ins", "delays-per-exec"}) {
        auto missing = sample;
        auto start = missing.find(std::string(key) + " ");
        missing.erase(start, missing.find('\n', start) - start + 1);
        check(!load(c, missing, &error) && error.find("Missing parameter") != std::string::npos, std::string("missing ") + key);
    }
    check(!load(c, ""), "reject empty file");
    for (const auto& key : {"num-cpu", "quantum-cycles", "batch-process-freq", "min-ins", "max-ins", "delays-per-exec"}) {
        check(!load(c, setting(sample, key, "-1")), std::string("reject negative ") + key);
        check(!load(c, setting(sample, key, "4294967297")), std::string("reject above bound ") + key);
        check(!load(c, setting(sample, key, "2junk")), std::string("reject partial numeric ") + key);
    }
    auto maximum = setting(sample, "num-cpu", "128");
    for (const auto& key : {"quantum-cycles", "batch-process-freq", "min-ins", "max-ins", "delays-per-exec"}) {
        maximum = setting(maximum, key, "4294967296");
    }
    check(load(c, maximum), "accept inclusive 2^32 bounds and 128 cores");
    check(!load(c, setting(sample, "num-cpu", "129")), "reject 129 cores");
    check(!load(c, setting(sample, "quantum-cycles", "0")), "reject zero quantum");
    check(!load(c, setting(sample, "batch-process-freq", "0")), "reject zero batch frequency");
    check(!load(c, setting(sample, "min-ins", "0")), "reject zero instruction count");
    check(!load(c, setting(sample, "min-ins", "6")), "reject reversed instruction range");
    check(!load(c, setting(sample, "scheduler", "priority")), "reject unsupported scheduler");
    check(!load(c, sample + "num-cpu 2\n"), "reject duplicate setting");
    check(!load(c, sample + "other 2\n"), "reject unknown setting");
    check(!load(c, sample + "dangling\n"), "reject setting without value");
    check(!load(c, setting(sample, "scheduler", "\"rr")), "reject unmatched quotes");
    check(!load(c, setting(sample, "num-cpu", "4 extra")), "reject extra tokens");
    check(load(c, sample), "valid config reloads after failure");
    check(!load(c, setting(sample, "num-cpu", "129")) && c.numCPU == 4, "failed load preserves previous config");
    check(!c.loadFromFile("build/test-data/missing.txt", &error) && error.find("Cannot open") != std::string::npos, "missing config error");
}
void instructionTests() {
    Process arithmetic(1, "math", {
        Instruction::makeDeclare("x", 65530), Instruction::makeAdd("x", "x", "10"), Instruction::makePrint("sum=+x"),
        Instruction::makeSubtract("x", "1", "4"), Instruction::makePrint("difference=+x"),
        Instruction::makeAdd("y", "undeclared", "5"), Instruction::makePrint("auto=+ y")
    }, 0);
    arithmetic.setRunning(1, 2);
    for (uint64_t t = 1; t <= 7; ++t) { arithmetic.executeOne(t, 0); }
    const auto logs = arithmetic.getLogs();
    check(logs.size() == 3 && logs[0].find("sum=65535") != std::string::npos, "addition clamps at uint16 maximum");
    check(logs[1].find("difference=0") != std::string::npos, "subtraction clamps at zero");
    check(logs[2].find("auto=5") != std::string::npos, "auto declaration and PRINT variable");
    check(logs[0].front() == '(' && logs[0].find(") Core:2 ") != std::string::npos, "PRINT timestamp and core ID");

    const auto nested = Instruction::makeFor({
        Instruction::makeAdd("x", "x", "1"),
        Instruction::makeFor({Instruction::makeFor({Instruction::makeAdd("x", "x", "1"), Instruction::makePrint("value=+x")}, 3)}, 2)
    }, 2);
    Process loop(2, "loop", {Instruction::makeDeclare("x", 0), nested, Instruction::makePrint("final=+x")}, 0);
    check(loop.getInstructionCount() == 28, "expanded loop workload count");
    for (uint64_t t = 1; t <= 28; ++t) {
        loop.setRunning(t, static_cast<int>(t % 2));
        loop.executeOne(t, 0);
        loop.setReady();
    }
    check(loop.getState() == ProcessState::Finished && loop.getExecutedInstructions() == 28, "three nested loops finish after exact workload");
    check(loop.getLogs().size() == 13 && loop.getLogs().back().find("final=14") != std::string::npos, "loop state survives repeated preemption");
    check(!loop.getSnapshot().finishTime.empty(), "finished timestamp retained");
    Process zero(3, "zero", {Instruction::makeFor({Instruction::makePrint("skip")}, 0),
        Instruction::makeFor({Instruction::makeFor({}, 2)}, std::numeric_limits<uint64_t>::max()), Instruction::makePrint("once")}, 0);
    zero.setRunning(1); zero.executeOne(1, 0);
    check(zero.getState() == ProcessState::Finished && zero.getLogs().size() == 1, "zero and empty loops skip immediately");
    bool rejected = false;
    try { countExecutableInstructions({Instruction::makeFor({nested}, 1)}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "reject fourth loop nesting level");
    rejected = false;
    try { countExecutableInstructions({Instruction::makeFor(prints(2), std::numeric_limits<uint64_t>::max())}); }
    catch (const std::overflow_error&) { rejected = true; }
    check(rejected, "reject expanded instruction overflow");
    for (uint64_t length : {uint64_t{1}, uint64_t{2}, uint64_t{20}, uint64_t{1000}, uint64_t{1} << 32}) {
        for (int i = 0; i < 10; ++i) {
            const auto generated = generateInstructions("random", length);
            check(countExecutableInstructions(generated) == length, "random programs preserve requested workload and valid nesting");
        }
    }
    check(generateInstructions("large", uint64_t{1} << 32).front().type == InstructionType::FOR, "large workloads use compact loops");
}
void schedulerTests() {
    {
        Config c; c.numCPU = 1; c.batchProcessFreq = 2; c.minInstructions = 1; c.maxInstructions = 1;
        Scheduler s; s.initialize(c);
        for (int t = 0; t < 3; ++t) { SchedulerTestAccess::tick(s); }
        s.startBatchGeneration(); SchedulerTestAccess::tick(s);
        check(s.getAllProcesses().empty(), "batch interval starts at scheduler-start");
        s.startBatchGeneration(); SchedulerTestAccess::tick(s);
        check(s.getAllProcesses().size() == 1, "repeated scheduler-start does not postpone generation");
        s.stopBatchGeneration(); SchedulerTestAccess::tick(s); s.startBatchGeneration();
        SchedulerTestAccess::tick(s);
        check(s.getAllProcesses().size() == 1, "restarted generator waits full interval");
        SchedulerTestAccess::tick(s);
        check(s.getAllProcesses().size() == 2, "restarted generator resumes at configured frequency");
    }
    for (const std::string algorithm : {"fcfs", "rr"}) {
        Config c; c.numCPU = 1; c.scheduler = algorithm; c.quantumCycles = 2; c.delaysPerExec = 0;
        Scheduler s; check(s.initialize(c), "initialize scheduler");
        auto a = SchedulerTestAccess::add(s, "A", prints(4));
        SchedulerTestAccess::add(s, "B", prints(2));
        std::string order;
        for (int t = 0; t < 6; ++t) {
            const auto before = a->getExecutedInstructions();
            SchedulerTestAccess::tick(s);
            order += a->getExecutedInstructions() > before ? 'A' : 'B';
        }
        check(order == (algorithm == "fcfs" ? "AAAABB" : "AABBAA"), algorithm + " scheduling order");
        check(Report::generate(s).find("CPU utilization: 0.00%") != std::string::npos, "idle utilization is zero");
    }
    {
        Config c; c.numCPU = 2; c.scheduler = "rr"; c.quantumCycles = 1; c.delaysPerExec = 0;
        Scheduler s; s.initialize(c);
        auto a = SchedulerTestAccess::add(s, "A", prints(3));
        auto b = SchedulerTestAccess::add(s, "B", prints(3));
        SchedulerTestAccess::tick(s);
        const auto snapshot = s.getSnapshot();
        check(a->getExecutedInstructions() == 1 && b->getExecutedInstructions() == 1, "two simulated cores advance in the same tick");
        check(snapshot.usedCores == 2 && snapshot.processes[0].coreID == 0 && snapshot.processes[1].coreID == 1, "core IDs and quantum-one occupancy");
        auto report = Report::generate(s);
        check(report.find("CPU utilization: 100.00%") != std::string::npos, "current utilization matches occupied cores");
        check(Report::save(report, "build/test-data/report.txt"), "save report");
        SchedulerTestAccess::tick(s);
        std::ifstream file("build/test-data/report.txt"); std::ostringstream saved; saved << file.rdbuf();
        check(saved.str() == report, "saved report keeps exact displayed snapshot even after another tick");
    }
    for (const std::string algorithm : {"fcfs", "rr"}) {
        Config c; c.numCPU = 1; c.scheduler = algorithm; c.quantumCycles = 1; c.delaysPerExec = 0;
        Scheduler s; s.initialize(c);
        auto sleeper = SchedulerTestAccess::add(s, "sleeper", {Instruction::makeFor({Instruction::makePrint("before"), Instruction::makeSleep(2), Instruction::makePrint("after")}, 2)});
        auto other = SchedulerTestAccess::add(s, "other", prints(10));
        SchedulerTestAccess::tick(s); SchedulerTestAccess::tick(s);
        const auto before = other->getExecutedInstructions();
        for (int t = 0; t < 40; ++t) { SchedulerTestAccess::tick(s); }
        const auto logs = sleeper->getLogs();
        check(sleeper->getState() == ProcessState::Finished && logs.size() == 4, algorithm + " nested SLEEP finishes");
        check(logs[0].find("before") != std::string::npos && logs[1].find("after") != std::string::npos && logs[2].find("before") != std::string::npos && logs[3].find("after") != std::string::npos, "loop continuation preserves PRINT order across sleep");
        check(other->getExecutedInstructions() > before, "other process progresses while sleeper yields");
    }
    {
        Config c; c.numCPU = 1; c.scheduler = "fcfs"; c.delaysPerExec = 0;
        Scheduler s; s.initialize(c);
        auto sleeper = SchedulerTestAccess::add(s, "last-sleep", {Instruction::makeSleep(2)});
        SchedulerTestAccess::tick(s);
        check(s.getAvailableCores() == 1 && sleeper->getState() == ProcessState::Sleeping, "SLEEP relinquishes core immediately");
        SchedulerTestAccess::tick(s); SchedulerTestAccess::tick(s);
        check(sleeper->getState() == ProcessState::Finished && sleeper->getFinishTick() == 3 && s.getAvailableCores() == 1, "last SLEEP finishes on wake without occupying another core tick");
    }
    {
        Config c; c.numCPU = 1; c.scheduler = "rr"; c.quantumCycles = 1; c.delaysPerExec = 2;
        Scheduler s; s.initialize(c); auto p = SchedulerTestAccess::add(s, "delay", prints(2));
        SchedulerTestAccess::tick(s); SchedulerTestAccess::tick(s); SchedulerTestAccess::tick(s);
        check(p->getExecutedInstructions() == 1 && s.getAvailableCores() == 0, "busy delay retains core and survives quantum expiry");
        SchedulerTestAccess::tick(s); check(p->getState() == ProcessState::Finished, "next instruction executes after configured delay");
    }
    {
        Config c; c.numCPU = 1; c.batchProcessFreq = 2; c.minInstructions = 1; c.maxInstructions = 1; c.delaysPerExec = 0;
        Scheduler s; s.initialize(c); check(s.createProcess("p01") != nullptr, "manual process creation");
        check(s.createProcess("p01") == nullptr, "duplicate process rejected");
        s.startBatchGeneration();
        for (int t = 0; t < 4; ++t) { SchedulerTestAccess::tick(s); }
        check(s.getAllProcesses().size() == 3 && s.findProcess("p02") && s.findProcess("p03"), "batch frequency and readable unique names");
        s.stopBatchGeneration();
        for (int t = 0; t < 20; ++t) { SchedulerTestAccess::tick(s); }
        check(s.getAllProcesses().size() == 3, "scheduler-stop stops generation");
        for (const auto& p : s.getAllProcesses()) { check(p->getState() == ProcessState::Finished, "existing processes continue after scheduler-stop"); }
    }
}
}
int main() {
    try {
        std::filesystem::create_directories("build/test-data");
        configTests(); std::cout << "PASS: config validation\n";
        instructionTests(); std::cout << "PASS: arithmetic, PRINT, FOR and workload counts\n";
        schedulerTests(); std::cout << "PASS: FCFS, RR, multicore, sleep, delays, batches and reports\n";
        std::cout << "All " << checks << " checks passed.\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
