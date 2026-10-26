# G-9 --- Integrated HOST Dispatcher

A C++/Qt desktop simulation of an operating-system HOST dispatcher. The
application demonstrates real-time scheduling, multilevel feedback
queues (MLFQ), process lifecycle management, memory allocation, I/O
blocking, suspension/resumption, and performance metrics.

## Project scope

This is an **educational simulator**, not a real kernel or production
operating-system dispatcher. It simulates processes and resources inside
the application. It must not claim to control actual OS processes,
allocate physical RAM, or operate real hardware devices.

## Start here

1.  Read `RULES.md` and `TECH_STACK.md`.
2.  Read `ARCHITECTURE.md` and `WORKFLOW.md`.
3.  Follow `CODEX_PHASE_PROMPTS.md` in order. Give Codex one phase at a
    time.
4.  Use `TESTING.md` to verify each phase.
5.  Use `USER_GUIDE.md` to run and demonstrate the completed
    application.

## Recommended implementation

-   C++17
-   Qt 6 Widgets
-   CMake
-   Qt Test for unit tests
-   JSON sample scenarios; persistence is optional and should not delay
    the core simulator

## Core features

-   Create, admit, suspend, resume, and terminate simulated processes.
-   Schedule real-time tasks with Earliest Deadline First (EDF).
-   Schedule user processes using a three-level MLFQ.
-   Simulate CPU ticks, preemption, I/O requests/completions, and memory
    allocation/release.
-   Show process state, queue membership, CPU timeline/Gantt chart,
    memory usage, I/O device queues, and performance metrics.
-   Provide deterministic sample scenarios and clear error messages.

## Important assumptions

-   The scheduler runs on a simulated clock; do not use real-time OS
    scheduling APIs.
-   Keep all scheduling/resource logic independent of Qt widgets.
-   Make scheduling rules and queue time quanta configurable where
    practical.
-   Document any unavoidable design decisions in `ARCHITECTURE.md`.
