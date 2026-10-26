# Codex Prompts --- Build in Phases

## How to use this file

Give Codex the contents of `RULES.md`, `TECH_STACK.md`,
`ARCHITECTURE.md`, and the current phase prompt. Work through phases in
order. Do not ask Codex to implement every phase in one giant request.
After each phase, inspect the diff, build, test, and commit the stable
result.

------------------------------------------------------------------------

## Phase 0 --- Repository inspection and plan

**Prompt to Codex:**

You are implementing G-9, an educational Integrated HOST Dispatcher
simulator. Read `RULES.md`, `TECH_STACK.md`, `ARCHITECTURE.md`,
`WORKFLOW.md`, and `TESTING.md`. Inspect the current repository before
changing anything. Do not implement the full application yet.

Return: 1. a concise architecture and implementation plan, 2. proposed
file tree, 3. Qt/CMake assumptions and any environment risks, 4.
acceptance criteria for each phase.

If the repository is empty, prepare a minimal C++17/Qt 6 Widgets/CMake
skeleton only. Ensure it builds. Do not implement later-phase features.
Report exact build commands and results.

## Phase 1 --- Process model and state machine

**Prompt to Codex:**

Implement the core domain model only. Create the Process model, process
type/state enums, validated process creation, unique IDs, timing fields,
deadline fields for real-time tasks, MLFQ level/quantum accounting, and
explicit legal state transitions. Keep this layer independent from UI
widgets. Add unit tests for validation and state transitions. Update
documentation if needed. Build and run tests. Do not implement
scheduling algorithms or UI yet.

Acceptance criteria: - invalid values and duplicate IDs are rejected, -
legal and illegal transitions are tested, - no owning raw pointers, -
project builds and tests pass.

## Phase 2 --- EDF and MLFQ scheduling

**Prompt to Codex:**

Implement `RealTimeScheduler` using Earliest Deadline First and
`MLFQScheduler` with Q0 RR quantum 2, Q1 RR quantum 4, and Q2 FCFS.
Implement deterministic tie-breaking, preemption support, queue
membership invariants, CPU-quantum accounting, demotion, I/O yield
behavior, and a configurable/documented priority-boost mechanism. Follow
`RULES.md`. Add deterministic tests for EDF ordering, real-time
preemption policy, MLFQ queue precedence, quantum expiry, demotion,
blocking/re-entry, suspension removal/re-entry, and duplicate queue
prevention. Do not implement GUI. Build and run all tests.

Acceptance criteria: - no suspended or blocked process is schedulable, -
queue order is deterministic, - a process appears in at most one ready
queue, - real-time-vs-user priority is implemented and documented.

## Phase 3 --- Memory and I/O resource managers

**Prompt to Codex:**

Implement `MemoryManager` and `IOManager`. Memory capacity must be
configurable; allocation should be atomic and tied to process admission,
and release must be safe and idempotent or clearly reject duplicate
release. Simulate devices and service durations using simulated time
only. I/O requests block a process; completion wakes it correctly,
including the SuspendedBlocked -\> SuspendedReady case. Do not access
actual hardware. Add tests for allocation, insufficient memory,
termination cleanup, device queues, completion, and suspension
interactions. Build and run all tests.

Acceptance criteria: - memory totals never become negative or exceed
capacity, - each admitted process has at most one allocation, - blocked
processes never receive CPU, - resource queues remain consistent after
termination/reset.

## Phase 4 --- Simulation controller, dispatcher, and metrics

**Prompt to Codex:**

Implement `HostDispatcher`, `SimulationController`, event/timeline
recording, and `MetricsCollector`. Coordinate the EDF and MLFQ
schedulers with the documented real-time precedence policy. Implement
one-tick stepping, simulated arrivals, I/O completion, CPU execution,
quantum expiry, process completion, deadline miss recording, memory
release, reset, and metrics. Document the precise event order and test
same-tick boundary cases. Do not build the full GUI yet; a small
test/demo runner is acceptable. Build and run all tests.

Acceptance criteria: - simulation is deterministic, - one Step advances
exactly one simulated tick, - reset returns all resources and metrics to
initial state, - CPU timeline agrees with CPU accounting, - waiting,
response, turnaround, utilization, throughput, and deadline misses are
computed from events.

## Phase 5 --- Qt Widgets application shell

**Prompt to Codex:**

Build the Qt Widgets main window and navigation using the existing
simulation API. Add dashboard summary cards, process table, scheduler
view, resource manager view, and performance view. Keep UI code thin; do
not put scheduling logic in widgets. Add a timer-driven Play/Pause
option and a Step button. Ensure the UI stays responsive and the timer
only advances simulated ticks. Use clear labels, input validation,
accessible contrast, sensible sizing, and helpful empty states. Build
and manually verify all screens. Do not add nonfunctional placeholder
buttons.

Acceptance criteria: - application opens and navigates without
crashes, - UI reflects controller state, - no scheduler/resource
decisions are implemented in the UI, - Play/Pause/Step/Reset have
working behavior.

## Phase 6 --- Process controls and input forms

**Prompt to Codex:**

Add the Add Process form with conditional fields for User and Real-Time
tasks. Add/adapt controls for admit, suspend, resume, and terminate.
Connect them to the domain/controller APIs. Disable invalid actions
based on process state and display useful validation/admission errors.
Add a sample scenario loader and reset confirmation. Add UI-level tests
where practical, plus integration tests for the underlying actions.
Build and verify all workflows.

Acceptance criteria: - process actions follow the state machine, -
insufficient memory does not partially admit a process, - termination
releases memory and removes queue/device entries, - sample scenario is
deterministic and immediately demonstrable.

## Phase 7 --- Timeline, resource visualization, and metrics

**Prompt to Codex:**

Implement the CPU Gantt/timeline visualization, MLFQ queue views, memory
used/free display, I/O active/waiting queues, and performance reports. A
custom QPainter widget is acceptable; avoid requiring an unavailable
chart dependency. Include CPU idle periods and process IDs in the
timeline. Calculate all metrics from the simulation history and handle
empty runs safely. Add tests for metric formulas and manual checks for
visual consistency. Build and run tests.

Acceptance criteria: - displayed values match the controller/event
log, - timeline updates on each tick and handles long runs sensibly, -
memory and I/O views show current simulated state, - no fake or
hardcoded metric values.

## Phase 8 --- Hardening, sample scenarios, and documentation

**Prompt to Codex:**

Perform a quality pass using `RULES.md` and `TESTING.md`. Add edge-case
tests for empty workloads, simultaneous arrivals/deadlines/I/O
completions, termination while blocked/suspended, reset during
execution, memory exhaustion, and priority boosts. Fix bugs rather than
hiding them. Improve error messages, keyboard/focus behavior, and
layout. Finalize README and user guide to match the actual
implementation. Ensure a clean configure/build/test succeeds. Report
known limitations honestly and provide a short demonstration script for
a project presentation.

Acceptance criteria: - clean build succeeds, - all tests pass, - manual
acceptance checklist is completed or explicitly lists remaining items, -
documentation accurately reflects what exists.

## Phase 9 --- Final review and submission package

**Prompt to Codex:**

Review the completed repository as a student project submission. Do not
add major new features. Check code quality, consistency,
license/dependency notes, build instructions, test coverage, sample
scenario, and screenshots/demo instructions. Verify that the app is
described as a simulation, not a real kernel dispatcher. Remove dead
code and unused dependencies. Ensure no generated build files or
machine-specific paths are committed. Return a final summary, exact
build/test commands, implemented features, limitations, and a 3--5
minute demo sequence.
