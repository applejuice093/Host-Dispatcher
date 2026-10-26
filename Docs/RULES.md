# Codex Project Rules

These instructions apply to every phase of the G-9 HOST Dispatcher
project.

## 1. Project goal and boundaries

Build a reliable, understandable **OS scheduling simulator** using C++
and Qt Widgets. This is not a kernel implementation. All processes, CPU
activity, memory, and I/O devices are simulated in application state.

## 2. Engineering rules

1.  Use C++17 and Qt 6 Widgets with CMake.
2.  Keep the project modular. Separate domain models, scheduling
    algorithms, resource managers, simulation controller, UI, and tests.
3.  Scheduling and resource-management logic must not depend on UI
    widgets or Qt event handlers.
4.  Use clear class/function names, small functions, const-correctness,
    and RAII.
5.  Prefer standard containers (`std::vector`, `std::deque`,
    `std::queue`, `std::unordered_map`) where appropriate. Avoid raw
    owning pointers.
6.  Do not add dependencies unless justified and documented.
7.  Do not introduce placeholder implementations, fake metrics, or
    buttons that do nothing. If a feature is not implemented, label it
    clearly.
8.  Do not silently swallow errors. Validate inputs and show
    understandable error messages.
9.  Keep the application buildable after each phase. Do not proceed by
    leaving compile errors.
10. Add or update unit tests for every scheduling or resource-management
    behavior.
11. Use deterministic tests; avoid relying on wall-clock timing.
12. Avoid global mutable state. Use a central simulation controller with
    explicit ownership.
13. Do not rewrite unrelated files or change public interfaces without a
    reason.
14. Update `README.md`, `ARCHITECTURE.md`, and `WORKFLOW.md` when
    implementation decisions materially change.

## 3. Process model

Each process/task should have a unique ID and at minimum: - name -
process type: real-time or user - state - arrival time - total CPU burst
/ remaining CPU time - memory requirement - accumulated waiting time -
first-start time, if started - completion time, if completed - I/O
request configuration or a testable I/O event schedule - for real-time
tasks: absolute deadline and deadline-miss status - for MLFQ tasks:
current queue level and consumed quantum

Use integer simulated time units. Clearly define whether deadlines are
absolute or relative; the recommended model stores an absolute deadline
computed from arrival time plus relative deadline.

## 4. State machine

Support at least: - New - Ready - Running - Blocked - SuspendedReady -
SuspendedBlocked - Terminated

Enforce legal state transitions through the simulation/domain layer, not
directly from UI code. A suspended process must not be selected by the
CPU scheduler. Resuming a suspended-ready process returns it to Ready.
Resuming a suspended-blocked process returns it to Blocked until its I/O
completes, unless the I/O has already completed, in which case it
becomes Ready.

## 5. Scheduling policy

### Real-time tasks

-   Use Earliest Deadline First (EDF) among eligible real-time tasks.
-   Define and document real-time-vs-user priority. Recommended default:
    eligible real-time tasks preempt user processes; EDF chooses among
    real-time tasks.
-   On a new eligible real-time task with an earlier deadline, preempt
    the current user task or later-deadline real-time task.
-   Record missed deadlines. Do not automatically treat a missed
    deadline as process termination unless the chosen policy explicitly
    says so.
-   Explain that EDF feasibility guarantees depend on assumptions; the
    simulator should not claim every task set is schedulable.

### User tasks: MLFQ

-   Use three queues: Q0, Q1, Q2.
-   Default policy: Q0 Round Robin quantum 2, Q1 Round Robin quantum 4,
    Q2 FCFS.
-   Higher-priority non-empty queues are selected first.
-   If a CPU-bound task consumes its full quantum, demote it one level
    (Q2 remains Q2).
-   If a task blocks for I/O before exhausting its quantum, do not
    demote it by default.
-   Implement a documented priority-boost mechanism or clearly document
    if priority boosting is deferred. Recommended default: boost waiting
    user tasks to Q0 after a configurable number of ticks.
-   Prevent duplicate queue entries when preempting, suspending,
    resuming, or completing processes.
-   Keep queue operations and time accounting testable.

## 6. Memory and I/O simulation

-   Memory capacity is configurable, with a reasonable default such as
    1024 MB.
-   Allocate memory when a process is admitted, not every CPU tick.
-   If memory is insufficient, reject admission with a clear reason or
    hold the process in a documented admission-wait state. Prefer clear
    rejection for the first version.
-   Release memory exactly once when a process terminates or is
    explicitly removed.
-   Simulate devices such as Disk, Printer, and Keyboard. Do not perform
    real hardware I/O.
-   A process requesting I/O transitions to Blocked and joins the
    corresponding device queue.
-   When simulated I/O completes, move the process to Ready unless it is
    suspended; in that case transition to SuspendedReady.
-   Define device service time and process I/O request rules in one
    place, and test them.

## 7. UI rules

-   Provide a usable Qt Widgets interface with clear navigation or tabs.
-   Show process ID, name, type, state, remaining CPU time, queue level,
    memory, and deadline where applicable.
-   Provide controls for add/admit, start, pause, step, reset, suspend,
    resume, and terminate.
-   Disable actions that are invalid for the selected process and show
    why when useful.
-   Display a simulated CPU timeline/Gantt chart and current tick.
-   Show memory capacity/used/free and I/O device queues.
-   Provide a sample scenario button so the project can be demonstrated
    immediately.
-   Do not block the UI with long-running loops. Advance simulation by
    one tick per timer event or explicit Step action.
-   Ensure the simulation can be paused and reset deterministically.

## 8. Metrics

Calculate metrics from recorded simulation events, not arbitrary
estimates: - average waiting time - average turnaround time - average
response time - CPU utilization over the simulated interval -
throughput - completed process count - deadline misses and deadline-miss
rate for real-time tasks

Document formulas and edge cases in `ARCHITECTURE.md` or `TESTING.md`.
Do not divide by zero when there are no eligible processes or no elapsed
simulation time.

## 9. Codex workflow

At the start of each phase: 1. Inspect the existing repository and
current build setup. 2. Summarize the intended changes. 3. Implement
only the current phase. 4. Build and run relevant tests. 5. Fix errors
before finishing. 6. List files changed, commands run, test results, and
any remaining limitations.

Do not jump ahead to later phases. Do not replace a working
implementation with a new architecture without explaining why.
