#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace host_dispatcher {

enum class ProcessType { User, RealTime };
enum class ProcessState {
    New, Ready, Running, Blocked, SuspendedReady, SuspendedBlocked, Terminated
};

struct IoRequest {
    std::string device = "Disk";
    int afterCpu = 0;
    int duration = 1;
};

struct ProcessSpec {
    int id = 0;
    std::string name;
    ProcessType type = ProcessType::User;
    int arrival = 0;
    int cpuBurst = 1;
    int memoryMb = 1;
    bool hasDeadline = false;
    int relativeDeadline = 0;
    bool hasIo = false;
    IoRequest io;
};

struct Process {
    ProcessSpec spec;
    ProcessState state = ProcessState::New;
    int remaining = 0;
    int queueLevel = 0;
    int quantumUsed = 0;
    int waitingTime = 0;
    int queueWaitTicks = 0;
    int cpuUsed = 0;
    int firstStart = -1;
    int completion = -1;
    bool completedNormally = false;
    bool userTerminated = false;
    bool ioIssued = false;
    bool deadlineMissed = false;
    bool ioCompletedWhileSuspended = false;
    bool admissionWaitLogged = false; // true once the "waiting for memory" event was recorded

    int absoluteDeadline() const;
};

std::string toString(ProcessType type);
std::string toString(ProcessState state);

class ProcessRegistry {
public:
    void add(const ProcessSpec& spec);
    void clear();
    bool contains(int id) const;
    Process& at(int id);
    const Process& at(int id) const;
    const std::map<int, Process>& all() const { return processes_; }
private:
    std::map<int, Process> processes_;
};

class MemoryManager {
public:
    explicit MemoryManager(int capacityMb = 1024);
    bool allocate(int id, int amountMb);
    bool release(int id);
    void clear();
    int capacity() const { return capacityMb_; }
    int used() const { return usedMb_; }
    int free() const { return capacityMb_ - usedMb_; }
private:
    int capacityMb_;
    int usedMb_ = 0;
    std::unordered_map<int, int> allocations_;
};

class IoManager {
public:
    struct Request { int processId; int due; int duration; };
    void request(const std::string& device, int processId, int now, int duration);
    std::vector<int> completeDue(int now);
    void remove(int processId, int now);
    void clear();
    std::string describe() const;
private:
    struct Device { bool hasActive = false; Request active{}; std::deque<Request> waiting; };
    std::map<std::string, Device> devices_;
    void startNext(Device& device, int now);
};

class RealTimeScheduler {
public:
    int selectEarliestReady(const ProcessRegistry& processes) const;
};

class MLFQScheduler {
public:
    void enqueue(Process& process, bool front = false);
    void remove(int id);
    int selectNext(const ProcessRegistry& processes);
    bool hasReadyAbove(int level) const;
    void clear();
    std::array<std::vector<int>, 3> readyQueues() const;
private:
    std::array<std::deque<int>, 3> queues_;
};

class HostDispatcher {
public:
    int select(ProcessRegistry& processes, const RealTimeScheduler& realTimeScheduler,
               MLFQScheduler& mlfqScheduler, int& currentProcessId) const;
};

struct TickRecord { int time; int processId; }; // processId 0 denotes idle
struct Metrics {
    int elapsed = 0;
    int completed = 0;
    int deadlineMisses = 0;
    int realTimeTasks = 0;
    double averageWaiting = 0.0;
    double averageTurnaround = 0.0;
    double averageResponse = 0.0;
    double utilizationPercent = 0.0;
    double throughput = 0.0;
    double deadlineMissRatePercent = 0.0;
};

class MetricsCollector {
public:
    Metrics collect(const ProcessRegistry& processes, const std::vector<TickRecord>& timeline, int elapsed) const;
};

class Simulation {
public:
    explicit Simulation(int memoryMb = 1024);
    void addProcess(const ProcessSpec& spec);
    void loadSample();
    void setMemoryCapacity(int capacityMb);
    void step();
    void run(int ticks);
    void reset();
    bool suspend(int id);
    bool resume(int id);
    bool terminate(int id);
    const ProcessRegistry& processes() const { return processes_; }
    const MemoryManager& memory() const { return memory_; }
    const IoManager& io() const { return io_; }
    const std::vector<TickRecord>& timeline() const { return timeline_; }
    const std::vector<std::string>& messages() const { return messages_; }
    int time() const { return time_; }
    void setPriorityBoostInterval(int ticks);
    Metrics metrics() const;
    std::array<std::vector<int>, 3> readyQueues() const;
private:
    ProcessRegistry processes_;
    MemoryManager memory_;
    IoManager io_;
    int time_ = 0;
    int current_ = 0;
    int priorityBoostInterval_ = 20;
    RealTimeScheduler realTimeScheduler_;
    MLFQScheduler mlfqScheduler_;
    HostDispatcher dispatcher_;
    std::vector<TickRecord> timeline_;
    std::vector<std::string> messages_;

    void enqueue(Process& process, bool front = false);
    void removeFromQueues(int id);
    int chooseNext();
    void admitArrivals();
    void wakeIo();
    void updateDeadlineMisses();
    void ageReadyProcesses(int exceptId = 0);
};

} // namespace host_dispatcher
