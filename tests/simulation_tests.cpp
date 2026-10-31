#include "simulation.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace host_dispatcher;

namespace {
int assertions = 0;
void check(bool condition, const std::string& message)
{
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}
ProcessSpec task(int id, ProcessType type, int burst, int arrival = 0, int deadline = 0)
{
    ProcessSpec spec;
    spec.id = id; spec.name = "P" + std::to_string(id); spec.type = type;
    spec.cpuBurst = burst; spec.arrival = arrival; spec.memoryMb = 10;
    if (type == ProcessType::RealTime) { spec.hasDeadline = true; spec.relativeDeadline = deadline; }
    return spec;
}
}

int main()
try {
    ProcessRegistry registry;
    registry.add(task(1, ProcessType::User, 3));
    check(registry.at(1).remaining == 3 && registry.at(1).state == ProcessState::New, "new task initialization");
    bool invalid = false;
    try { registry.add(task(1, ProcessType::User, 3)); } catch (const std::invalid_argument&) { invalid = true; }
    check(invalid, "duplicate process ID rejected");
    invalid = false;
    try { registry.add(task(2, ProcessType::RealTime, 0, 0, 3)); } catch (const std::invalid_argument&) { invalid = true; }
    check(invalid, "zero CPU burst rejected");
    invalid = false;
    try {
        ProcessSpec overflow = task(3, ProcessType::RealTime, 1, std::numeric_limits<int>::max(), 1);
        registry.add(overflow);
    } catch (const std::invalid_argument&) { invalid = true; }
    check(invalid, "absolute deadline overflow rejected");

    MemoryManager memory(100);
    check(memory.allocate(1, 60) && memory.used() == 60 && memory.free() == 40, "memory allocation accounting");
    check(!memory.allocate(2, 50) && memory.used() == 60, "insufficient memory is atomic");
    check(memory.release(1) && !memory.release(1) && memory.used() == 0, "release is safe and idempotent");

    Simulation configurable(512);
    configurable.setMemoryCapacity(256);
    check(configurable.memory().capacity() == 256, "simulated memory capacity is configurable before a run");
    configurable.step();
    bool capacityLocked = false;
    try { configurable.setMemoryCapacity(128); } catch (const std::logic_error&) { capacityLocked = true; }
    check(capacityLocked, "memory capacity cannot change after simulated time starts");

    Simulation edf;
    edf.addProcess(task(1, ProcessType::RealTime, 3, 0, 10));
    edf.addProcess(task(2, ProcessType::RealTime, 1, 0, 2));
    edf.step();
    check(edf.timeline().front().processId == 2, "EDF chooses earliest absolute deadline");
    Simulation tie;
    tie.addProcess(task(2, ProcessType::RealTime, 1, 0, 5));
    tie.addProcess(task(1, ProcessType::RealTime, 1, 0, 5));
    tie.step();
    check(tie.timeline().front().processId == 1, "EDF deadline ties use process ID deterministically");

    Simulation preemption;
    preemption.addProcess(task(1, ProcessType::User, 5));
    preemption.step();
    ProcessSpec urgent = task(2, ProcessType::RealTime, 1, 1, 3);
    preemption.addProcess(urgent);
    preemption.step();
    check(preemption.timeline()[1].processId == 2, "real-time task preempts user task");

    Simulation mlfq;
    mlfq.addProcess(task(1, ProcessType::User, 8));
    mlfq.addProcess(task(2, ProcessType::User, 1));
    mlfq.run(3);
    check(mlfq.timeline()[0].processId == 1 && mlfq.timeline()[1].processId == 1 &&
          mlfq.processes().at(1).queueLevel == 1 && mlfq.timeline()[2].processId == 2,
          "Q0 round robin quantum demotes and rotates");
    Simulation q1;
    q1.addProcess(task(1, ProcessType::User, 10));
    q1.run(6);
    check(q1.processes().at(1).queueLevel == 2, "Q1 full quantum demotes to Q2");

    Simulation boosting;
    boosting.setPriorityBoostInterval(1);
    boosting.addProcess(task(1, ProcessType::User, 30));
    boosting.addProcess(task(2, ProcessType::User, 30));
    boosting.run(10);
    check(boosting.processes().at(2).queueLevel == 0, "waiting lower-queue task receives a configurable priority boost");
    boosting.step();
    check(boosting.timeline().back().processId == 2, "boosted Q0 task preempts Q2 work");
    const auto boostedQueues = boosting.readyQueues();
    int duplicateBoostEntryCount = 0;
    for (const auto& queue : boostedQueues)
        for (int id : queue) if (id == 2) ++duplicateBoostEntryCount;
    check(duplicateBoostEntryCount <= 1, "priority boost preserves unique ready queue membership");

    Simulation io;
    ProcessSpec withIo = task(1, ProcessType::User, 4);
    withIo.hasIo = true; withIo.io = IoRequest{"Disk", 1, 2}; io.addProcess(withIo);
    io.step();
    check(io.processes().at(1).state == ProcessState::Blocked, "I/O request blocks running process");
    check(io.suspend(1) && io.processes().at(1).state == ProcessState::SuspendedBlocked, "blocked process can be suspended");
    io.run(2);
    check(io.processes().at(1).state == ProcessState::SuspendedReady, "I/O completes while suspended");
    check(io.resume(1) && io.processes().at(1).state == ProcessState::Ready, "suspended I/O completion resumes ready");

    IoManager devices;
    devices.request("Disk", 1, 0, 2); devices.request("Disk", 2, 0, 3);
    check(devices.describe().find("P2") != std::string::npos, "busy device queues a second request");
    bool duplicateIo = false;
    try { devices.request("Printer", 1, 0, 1); } catch (const std::invalid_argument&) { duplicateIo = true; }
    check(duplicateIo, "duplicate I/O request for one process is rejected");
    check(devices.completeDue(2).size() == 1 && devices.completeDue(4).empty() && devices.completeDue(5).size() == 1,
          "device serves queued requests serially for their configured durations");

    Simulation metrics;
    metrics.addProcess(task(1, ProcessType::User, 2));
    metrics.run(3);
    const Metrics values = metrics.metrics();
    check(values.completed == 1 && values.elapsed == 3, "completion and elapsed metrics");
    check(values.utilizationPercent > 66.6 && values.utilizationPercent < 66.7, "CPU utilization from timeline");
    check(values.averageTurnaround == 2.0 && values.averageResponse == 0.0, "turnaround and response formulas");

    Simulation deadlines;
    deadlines.addProcess(task(1, ProcessType::RealTime, 3, 0, 1));
    deadlines.step();
    check(deadlines.processes().at(1).deadlineMissed && deadlines.metrics().deadlineMisses == 1,
          "unfinished real-time task is marked late at its deadline");
    deadlines.step();
    check(deadlines.metrics().deadlineMisses == 1, "deadline miss is counted once per task");
    check(deadlines.metrics().deadlineMissRatePercent == 100.0, "deadline miss rate uses real-time tasks as its denominator");
    Simulation onTime;
    onTime.addProcess(task(1, ProcessType::RealTime, 1, 0, 1));
    onTime.step();
    check(!onTime.processes().at(1).deadlineMissed, "completion exactly at deadline is on time");

    Simulation admission(20);
    ProcessSpec memoryHolder = task(1, ProcessType::User, 1);
    memoryHolder.memoryMb = 15;
    admission.addProcess(memoryHolder);
    ProcessSpec waitsForMemory = task(2, ProcessType::User, 3);
    waitsForMemory.memoryMb = 10;
    admission.addProcess(waitsForMemory);
    admission.run(2);
    check(admission.memory().used() == 10 && admission.processes().at(2).state != ProcessState::New,
          "admission retries when completed work releases memory");

    Simulation crowded(10);
    ProcessSpec holder = task(1, ProcessType::User, 5); holder.memoryMb = 10; crowded.addProcess(holder);
    ProcessSpec waiterA = task(2, ProcessType::User, 1); crowded.addProcess(waiterA);
    ProcessSpec waiterB = task(3, ProcessType::User, 1); crowded.addProcess(waiterB);
    crowded.run(4);
    int admissionMessages = 0;
    for (const auto& message : crowded.messages())
        if (message.find("admission waiting") != std::string::npos) ++admissionMessages;
    check(admissionMessages == 2, "each waiting process logs its admission wait once, even when several wait");

    Simulation cleanup;
    ProcessSpec blocked = task(1, ProcessType::User, 4);
    blocked.hasIo = true; blocked.io = IoRequest{"Disk", 1, 10}; cleanup.addProcess(blocked);
    cleanup.step();
    check(cleanup.terminate(1) && cleanup.memory().used() == 0 && cleanup.io().describe() == "No active I/O requests",
          "termination removes blocked I/O and releases memory exactly once");

    Simulation empty;
    const Metrics emptyMetrics = empty.metrics();
    check(emptyMetrics.elapsed == 0 && emptyMetrics.completed == 0 && emptyMetrics.utilizationPercent == 0.0 && emptyMetrics.throughput == 0.0,
          "empty metrics avoid division by zero");
    empty.step();
    check(empty.metrics().utilizationPercent == 0.0 && empty.timeline().front().processId == 0,
          "idle elapsed interval has zero utilization and an idle timeline entry");

    Simulation reset;
    reset.loadSample(); reset.run(4); reset.reset();
    check(reset.time() == 0 && reset.processes().all().empty() && reset.memory().used() == 0 && reset.timeline().empty(),
          "reset clears simulation state and resources");

    std::cout << "Passed " << assertions << " simulation checks.\n";
    return EXIT_SUCCESS;
} catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return EXIT_FAILURE;
}
