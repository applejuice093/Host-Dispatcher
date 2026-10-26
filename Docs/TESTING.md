# Testing and Acceptance Criteria

## Automated core checks

The dependency-light `simulation_tests` executable covers process creation and validation, duplicate IDs, EDF ordering and ties, real-time preemption, Q0/Q1 demotion, priority boosts and queue uniqueness, memory allocation and release, insufficient-memory retry, I/O blocking/queue service/suspended completion, termination cleanup, deadlines and miss rate, metric formulas, empty metrics, and reset.

Build and run from the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Qt 6 Widgets 6.2+ enables the desktop target. Without Qt, CMake warns and builds the core and tests only. GUI smoke launch can use the `offscreen` Qt platform plugin; interactive visual checks should be performed on a desktop session.

## Manual acceptance checklist

- [ ] The app opens and displays the dashboard.
- [ ] Load Sample fills the table and can be reset.
- [ ] Play/Pause and Step advance and stop the simulated clock as expected.
- [ ] Add Process validates user and real-time fields and optional I/O configuration.
- [ ] Suspend, Resume, and Terminate enable for valid states and update process state.
- [ ] A real-time task preempts user work, and EDF order is visible.
- [ ] MLFQ queue order, demotion, and priority boosts are deterministic.
- [ ] Blocked and suspended processes do not receive CPU time.
- [ ] Timeline includes one process or idle entry per tick.
- [ ] Memory is released on normal completion and termination.
- [ ] I/O queues show active and waiting requests and completion wakes the process.
- [ ] Displayed metrics agree with the process records and timeline.
- [ ] Invalid inputs and insufficient memory produce understandable feedback.
- [ ] Reset clears resources, timeline, events, process records, and metrics.

The core and GUI targets build with Qt 6.8.3 and MinGW 13.1 in the verified environment; the automated core test suite passes. An offscreen startup smoke check was run. The interactive checklist above still needs human visual verification on a desktop session.
