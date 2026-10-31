#include "simulation.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace host_dispatcher {
namespace {
bool canTransition(ProcessState from, ProcessState to)
{
    switch (from) {
    case ProcessState::New: return to == ProcessState::Ready || to == ProcessState::Terminated;
    case ProcessState::Ready: return to == ProcessState::Running || to == ProcessState::SuspendedReady || to == ProcessState::Terminated;
    case ProcessState::Running: return to == ProcessState::Ready || to == ProcessState::Blocked || to == ProcessState::SuspendedReady || to == ProcessState::Terminated;
    case ProcessState::Blocked: return to == ProcessState::Ready || to == ProcessState::SuspendedBlocked || to == ProcessState::Terminated;
    case ProcessState::SuspendedReady: return to == ProcessState::Ready || to == ProcessState::Terminated;
    case ProcessState::SuspendedBlocked: return to == ProcessState::Blocked || to == ProcessState::SuspendedReady || to == ProcessState::Ready || to == ProcessState::Terminated;
    case ProcessState::Terminated: return false;
    }
    return false;
}

void transition(Process& process, ProcessState next)
{
    if (!canTransition(process.state, next))
        throw std::logic_error("Illegal process state transition from " + toString(process.state) + " to " + toString(next));
    process.state = next;
}
}

int Process::absoluteDeadline() const
{
    return spec.hasDeadline ? spec.arrival + spec.relativeDeadline : -1;
}

std::string toString(ProcessType type)
{
    return type == ProcessType::RealTime ? "Real-Time" : "User";
}

std::string toString(ProcessState state)
{
    switch (state) {
    case ProcessState::New: return "New";
    case ProcessState::Ready: return "Ready";
    case ProcessState::Running: return "Running";
    case ProcessState::Blocked: return "Blocked";
    case ProcessState::SuspendedReady: return "Suspended Ready";
    case ProcessState::SuspendedBlocked: return "Suspended Blocked";
    case ProcessState::Terminated: return "Terminated";
    }
    return "Unknown";
}

void ProcessRegistry::add(const ProcessSpec& spec)
{
    if (spec.id <= 0) throw std::invalid_argument("Process ID must be a positive integer.");
    if (processes_.count(spec.id)) throw std::invalid_argument("Process ID is already in use.");
    if (spec.name.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::invalid_argument("Process name cannot be empty.");
    if (spec.arrival < 0) throw std::invalid_argument("Arrival time cannot be negative.");
    if (spec.cpuBurst <= 0) throw std::invalid_argument("CPU burst must be greater than zero.");
    if (spec.memoryMb <= 0) throw std::invalid_argument("Memory request must be greater than zero.");
    if (spec.type == ProcessType::RealTime && (!spec.hasDeadline || spec.relativeDeadline <= 0))
        throw std::invalid_argument("Real-time tasks need a positive relative deadline.");
    if (spec.hasDeadline && spec.relativeDeadline > std::numeric_limits<int>::max() - spec.arrival)
        throw std::invalid_argument("Absolute deadline exceeds the supported simulated time range.");
    if (spec.type == ProcessType::User && spec.hasDeadline)
        throw std::invalid_argument("Only real-time tasks may have a deadline.");
    if (spec.hasIo) {
        if (spec.io.device.empty() || spec.io.duration <= 0 || spec.io.afterCpu <= 0 || spec.io.afterCpu >= spec.cpuBurst)
            throw std::invalid_argument("I/O needs a device, positive duration, and an offset before CPU completion.");
    }
    Process process;
    process.spec = spec;
    process.remaining = spec.cpuBurst;
    processes_.emplace(spec.id, process);
}

void ProcessRegistry::clear() { processes_.clear(); }
bool ProcessRegistry::contains(int id) const { return processes_.count(id) != 0; }
Process& ProcessRegistry::at(int id) { return processes_.at(id); }
const Process& ProcessRegistry::at(int id) const { return processes_.at(id); }

MemoryManager::MemoryManager(int capacityMb) : capacityMb_(capacityMb)
{
    if (capacityMb <= 0) throw std::invalid_argument("Memory capacity must be positive.");
}

bool MemoryManager::allocate(int id, int amountMb)
{
    if (id <= 0 || amountMb <= 0 || allocations_.count(id) || amountMb > free()) return false;
    allocations_.emplace(id, amountMb);
    usedMb_ += amountMb;
    return true;
}

bool MemoryManager::release(int id)
{
    const auto found = allocations_.find(id);
    if (found == allocations_.end()) return false;
    usedMb_ -= found->second;
    allocations_.erase(found);
    return true;
}

void MemoryManager::clear() { allocations_.clear(); usedMb_ = 0; }

void IoManager::startNext(Device& device, int now)
{
    if (device.hasActive || device.waiting.empty()) return;
    const auto request = device.waiting.front();
    device.waiting.pop_front();
    device.active = Request{request.processId, now + request.duration, request.duration};
    device.hasActive = true;
}

void IoManager::request(const std::string& deviceName, int processId, int now, int duration)
{
    if (deviceName.empty() || processId <= 0 || now < 0 || duration <= 0 ||
        duration > std::numeric_limits<int>::max() - now)
        throw std::invalid_argument("Invalid simulated I/O request.");
    for (const auto& entry : devices_) {
        const Device& device = entry.second;
        if (device.hasActive && device.active.processId == processId)
            throw std::invalid_argument("Process already has a simulated I/O request.");
        for (const auto& queued : device.waiting)
            if (queued.processId == processId)
                throw std::invalid_argument("Process already has a simulated I/O request.");
    }
    auto& device = devices_[deviceName];
    const Request request{processId, now + duration, duration};
    if (!device.hasActive) { device.active = request; device.hasActive = true; }
    else device.waiting.push_back(request);
}

std::vector<int> IoManager::completeDue(int now)
{
    std::vector<int> completed;
    for (auto& entry : devices_) {
        auto& device = entry.second;
        if (device.hasActive && device.active.due <= now) {
            completed.push_back(device.active.processId);
            device.hasActive = false;
            startNext(device, now);
        }
    }
    return completed;
}

void IoManager::remove(int processId, int now)
{
    for (auto& entry : devices_) {
        auto& device = entry.second;
        if (device.hasActive && device.active.processId == processId) {
            device.hasActive = false;
            startNext(device, now);
        }
        device.waiting.erase(std::remove_if(device.waiting.begin(), device.waiting.end(),
            [processId](const Request& request) { return request.processId == processId; }), device.waiting.end());
    }
}

void IoManager::clear() { devices_.clear(); }

std::string IoManager::describe() const
{
    std::ostringstream out;
    bool hasRequests = false;
    for (const auto& entry : devices_) {
        if (!entry.second.hasActive && entry.second.waiting.empty()) continue;
        hasRequests = true;
        out << entry.first << ": ";
        if (entry.second.hasActive) out << "P" << entry.second.active.processId << " active";
        else out << "idle";
        out << " | waiting";
        for (const auto& request : entry.second.waiting) out << " P" << request.processId;
        out << '\n';
    }
    return hasRequests ? out.str() : "No active I/O requests";
}

int RealTimeScheduler::selectEarliestReady(const ProcessRegistry& processes) const
{
    int earliest = 0;
    for (const auto& entry : processes.all()) {
        const Process& process = entry.second;
        if (process.spec.type != ProcessType::RealTime || process.state != ProcessState::Ready) continue;
        if (!earliest || process.absoluteDeadline() < processes.at(earliest).absoluteDeadline() ||
            (process.absoluteDeadline() == processes.at(earliest).absoluteDeadline() &&
             (process.spec.arrival < processes.at(earliest).spec.arrival ||
              (process.spec.arrival == processes.at(earliest).spec.arrival && process.spec.id < earliest))))
            earliest = process.spec.id;
    }
    return earliest;
}

void MLFQScheduler::enqueue(Process& process, bool front)
{
    if (process.spec.type != ProcessType::User || process.state != ProcessState::Ready ||
        process.queueLevel < 0 || process.queueLevel >= static_cast<int>(queues_.size())) return;
    remove(process.spec.id);
    auto& queue = queues_[static_cast<std::size_t>(process.queueLevel)];
    if (front) queue.push_front(process.spec.id); else queue.push_back(process.spec.id);
}

void MLFQScheduler::remove(int id)
{
    for (auto& queue : queues_)
        queue.erase(std::remove(queue.begin(), queue.end(), id), queue.end());
}

int MLFQScheduler::selectNext(const ProcessRegistry& processes)
{
    for (auto& queue : queues_) {
        while (!queue.empty()) {
            const int id = queue.front();
            queue.pop_front();
            if (processes.contains(id) && processes.at(id).state == ProcessState::Ready &&
                processes.at(id).spec.type == ProcessType::User) return id;
        }
    }
    return 0;
}

bool MLFQScheduler::hasReadyAbove(int level) const
{
    for (int queue = 0; queue < level && queue < static_cast<int>(queues_.size()); ++queue)
        if (!queues_[static_cast<std::size_t>(queue)].empty()) return true;
    return false;
}

void MLFQScheduler::clear()
{
    for (auto& queue : queues_) queue.clear();
}

std::array<std::vector<int>, 3> MLFQScheduler::readyQueues() const
{
    std::array<std::vector<int>, 3> result;
    for (std::size_t i = 0; i < queues_.size(); ++i)
        result[i].assign(queues_[i].begin(), queues_[i].end());
    return result;
}

int HostDispatcher::select(ProcessRegistry& processes, const RealTimeScheduler& realTimeScheduler,
                           MLFQScheduler& mlfqScheduler, int& currentProcessId) const
{
    const int earliest = realTimeScheduler.selectEarliestReady(processes);
    if (currentProcessId) {
        Process& current = processes.at(currentProcessId);
        bool preempt = current.state != ProcessState::Running;
        if (!preempt && earliest && current.spec.type != ProcessType::RealTime) preempt = true;
        else if (!preempt && earliest && current.spec.type == ProcessType::RealTime) {
            const Process& candidate = processes.at(earliest);
            preempt = candidate.absoluteDeadline() < current.absoluteDeadline() ||
                (candidate.absoluteDeadline() == current.absoluteDeadline() &&
                 (candidate.spec.arrival < current.spec.arrival ||
                  (candidate.spec.arrival == current.spec.arrival && candidate.spec.id < current.spec.id)));
        } else if (!preempt && current.spec.type == ProcessType::User) {
            preempt = mlfqScheduler.hasReadyAbove(current.queueLevel);
        }
        if (preempt) {
            if (current.state == ProcessState::Running) transition(current, ProcessState::Ready);
            if (current.spec.type == ProcessType::User) mlfqScheduler.enqueue(current, true);
            currentProcessId = 0;
        }
    }
    if (currentProcessId) return currentProcessId;
    if (earliest) return earliest;
    return mlfqScheduler.selectNext(processes);
}

Simulation::Simulation(int memoryMb) : memory_(memoryMb) {}

void Simulation::addProcess(const ProcessSpec& spec) { processes_.add(spec); }
void Simulation::setMemoryCapacity(int capacityMb)
{
    if (!processes_.all().empty() || time_ != 0)
        throw std::logic_error("Memory capacity can only be changed before a simulation run starts.");
    memory_ = MemoryManager(capacityMb);
}
void Simulation::setPriorityBoostInterval(int ticks)
{
    if (ticks < 0) throw std::invalid_argument("Priority boost interval cannot be negative; use zero to disable it.");
    priorityBoostInterval_ = ticks;
}

void Simulation::loadSample()
{
    reset();
    ProcessSpec sensor; sensor.id = 1; sensor.name = "RT sensor"; sensor.type = ProcessType::RealTime;
    sensor.cpuBurst = 5; sensor.memoryMb = 64; sensor.hasDeadline = true; sensor.relativeDeadline = 7;
    sensor.hasIo = true; sensor.io = IoRequest{"Disk", 2, 2}; addProcess(sensor);
    ProcessSpec interactive; interactive.id = 2; interactive.name = "Interactive";
    interactive.cpuBurst = 7; interactive.memoryMb = 128; interactive.hasIo = true;
    interactive.io = IoRequest{"Keyboard", 2, 1}; addProcess(interactive);
    ProcessSpec batch; batch.id = 3; batch.name = "Batch job"; batch.arrival = 1;
    batch.cpuBurst = 10; batch.memoryMb = 256; addProcess(batch);
    ProcessSpec control; control.id = 4; control.name = "RT control"; control.type = ProcessType::RealTime;
    control.arrival = 3; control.cpuBurst = 3; control.memoryMb = 64; control.hasDeadline = true;
    control.relativeDeadline = 5; addProcess(control);
}

void Simulation::enqueue(Process& process, bool front)
{
    if (process.spec.type != ProcessType::User || process.state != ProcessState::Ready) return;
    removeFromQueues(process.spec.id);
    mlfqScheduler_.enqueue(process, front);
}

void Simulation::removeFromQueues(int id)
{
    mlfqScheduler_.remove(id);
}

void Simulation::admitArrivals()
{
    for (const auto& entry : processes_.all()) {
        auto& process = processes_.at(entry.first);
        if (process.state != ProcessState::New || process.spec.arrival > time_) continue;
        if (!memory_.allocate(process.spec.id, process.spec.memoryMb)) {
            // Log once per process; comparing with only the last message spammed the
            // event log every tick whenever two or more processes were waiting.
            if (!process.admissionWaitLogged) {
                messages_.push_back("P" + std::to_string(process.spec.id) + " admission waiting: insufficient simulated memory.");
                process.admissionWaitLogged = true;
            }
            continue;
        }
        transition(process, ProcessState::Ready);
        process.queueWaitTicks = 0;
        enqueue(process);
    }
}

void Simulation::wakeIo()
{
    for (int id : io_.completeDue(time_)) {
        if (!processes_.contains(id)) continue;
        auto& process = processes_.at(id);
        if (process.state == ProcessState::Blocked) {
            transition(process, ProcessState::Ready);
            process.quantumUsed = 0;
            process.queueWaitTicks = 0;
            enqueue(process);
        } else if (process.state == ProcessState::SuspendedBlocked) {
            transition(process, ProcessState::SuspendedReady);
            process.ioCompletedWhileSuspended = true;
        }
    }
}

void Simulation::updateDeadlineMisses()
{
    for (const auto& entry : processes_.all()) {
        auto& process = processes_.at(entry.first);
        if (process.spec.type == ProcessType::RealTime && process.state != ProcessState::Terminated &&
            !process.deadlineMissed && time_ >= process.absoluteDeadline()) {
            process.deadlineMissed = true;
            messages_.push_back("P" + std::to_string(process.spec.id) + " missed its deadline.");
        }
    }
}

void Simulation::ageReadyProcesses(int exceptId)
{
    for (const auto& entry : processes_.all()) {
        Process& process = processes_.at(entry.first);
        if (entry.first == exceptId || process.state != ProcessState::Ready) continue;
        ++process.waitingTime;
        if (process.spec.type == ProcessType::User && process.queueLevel > 0 && priorityBoostInterval_ > 0 &&
            ++process.queueWaitTicks >= priorityBoostInterval_) {
            process.queueLevel = 0;
            process.quantumUsed = 0;
            process.queueWaitTicks = 0;
            enqueue(process, true);
            messages_.push_back("P" + std::to_string(process.spec.id) + " received an MLFQ priority boost.");
        }
    }
}

int Simulation::chooseNext()
{
    return dispatcher_.select(processes_, realTimeScheduler_, mlfqScheduler_, current_);
}

void Simulation::step()
{
    admitArrivals();
    wakeIo();
    updateDeadlineMisses();
    int selected = chooseNext();
    if (!selected) {
        timeline_.push_back({time_, 0});
        ++time_;
        ageReadyProcesses();
        updateDeadlineMisses();
        return;
    }

    auto& process = processes_.at(selected);
    if (process.state == ProcessState::Ready) transition(process, ProcessState::Running);
    process.queueWaitTicks = 0;
    current_ = selected;
    if (process.firstStart < 0) process.firstStart = time_;
    --process.remaining;
    ++process.cpuUsed;
    if (process.spec.type == ProcessType::User) ++process.quantumUsed;
    timeline_.push_back({time_, selected});

    if (process.remaining == 0) {
        transition(process, ProcessState::Terminated);
        process.completion = time_ + 1;
        process.completedNormally = true;
        memory_.release(selected);
        io_.remove(selected, time_);
        removeFromQueues(selected);
        current_ = 0;
    } else if (process.spec.hasIo && !process.ioIssued && process.cpuUsed == process.spec.io.afterCpu) {
        process.ioIssued = true;
        process.quantumUsed = 0;
        process.queueWaitTicks = 0;
        transition(process, ProcessState::Blocked);
        io_.request(process.spec.io.device, selected, time_, process.spec.io.duration);
        current_ = 0;
    } else if (process.spec.type == ProcessType::User && process.queueLevel < 2 &&
               process.quantumUsed >= (process.queueLevel == 0 ? 2 : 4)) {
        ++process.queueLevel;
        process.quantumUsed = 0;
        process.queueWaitTicks = 0;
        transition(process, ProcessState::Ready);
        enqueue(process);
        current_ = 0;
    }

    ++time_;
    ageReadyProcesses(selected);
    updateDeadlineMisses();
}

void Simulation::run(int ticks)
{
    if (ticks < 0) throw std::invalid_argument("Tick count cannot be negative.");
    for (int i = 0; i < ticks; ++i) step();
}

bool Simulation::suspend(int id)
{
    if (!processes_.contains(id)) return false;
    auto& process = processes_.at(id);
    removeFromQueues(id);
    switch (process.state) {
    case ProcessState::Ready: transition(process, ProcessState::SuspendedReady); break;
    case ProcessState::Running: transition(process, ProcessState::SuspendedReady); current_ = 0; break;
    case ProcessState::Blocked: transition(process, ProcessState::SuspendedBlocked); break;
    default: return false;
    }
    return true;
}

bool Simulation::resume(int id)
{
    if (!processes_.contains(id)) return false;
    auto& process = processes_.at(id);
    if (process.state == ProcessState::SuspendedReady) {
        transition(process, ProcessState::Ready);
        process.queueWaitTicks = 0;
        process.ioCompletedWhileSuspended = false;
        enqueue(process);
        return true;
    }
    if (process.state == ProcessState::SuspendedBlocked) {
        transition(process, process.ioCompletedWhileSuspended ? ProcessState::Ready : ProcessState::Blocked);
        process.queueWaitTicks = 0;
        process.ioCompletedWhileSuspended = false;
        enqueue(process);
        return true;
    }
    return false;
}

bool Simulation::terminate(int id)
{
    if (!processes_.contains(id)) return false;
    auto& process = processes_.at(id);
    if (process.state == ProcessState::Terminated) return false;
    transition(process, ProcessState::Terminated);
    process.userTerminated = true;
    if (process.completion < 0) process.completion = time_;
    memory_.release(id);
    io_.remove(id, time_);
    removeFromQueues(id);
    if (current_ == id) current_ = 0;
    return true;
}

void Simulation::reset()
{
    processes_.clear();
    memory_.clear();
    io_.clear();
    time_ = 0;
    current_ = 0;
    mlfqScheduler_.clear();
    timeline_.clear();
    messages_.clear();
}

Metrics MetricsCollector::collect(const ProcessRegistry& processes, const std::vector<TickRecord>& timeline, int elapsed) const
{
    Metrics result;
    result.elapsed = elapsed;
    int busy = 0;
    for (const auto& record : timeline) if (record.processId != 0) ++busy;
    result.utilizationPercent = elapsed ? 100.0 * busy / elapsed : 0.0;
    for (const auto& entry : processes.all()) {
        if (entry.second.spec.type != ProcessType::RealTime ||
            (entry.second.userTerminated && !entry.second.deadlineMissed)) continue;
        ++result.realTimeTasks;
        if (entry.second.deadlineMissed) ++result.deadlineMisses;
    }
    if (result.realTimeTasks)
        result.deadlineMissRatePercent = 100.0 * result.deadlineMisses / result.realTimeTasks;
    int waiting = 0, turnaround = 0, response = 0;
    for (const auto& entry : processes.all()) {
        const auto& process = entry.second;
        if (process.completedNormally && process.completion >= 0) {
            ++result.completed;
            waiting += process.waitingTime;
            turnaround += process.completion - process.spec.arrival;
            response += std::max(0, process.firstStart - process.spec.arrival);
        }
    }
    if (result.completed) {
        result.averageWaiting = static_cast<double>(waiting) / result.completed;
        result.averageTurnaround = static_cast<double>(turnaround) / result.completed;
        result.averageResponse = static_cast<double>(response) / result.completed;
    }
    result.throughput = elapsed ? static_cast<double>(result.completed) / elapsed : 0.0;
    return result;
}

Metrics Simulation::metrics() const
{
    return MetricsCollector().collect(processes_, timeline_, time_);
}

std::array<std::vector<int>, 3> Simulation::readyQueues() const
{
    return mlfqScheduler_.readyQueues();
}

} // namespace host_dispatcher
