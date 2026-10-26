# Development Workflow

## Current implementation status

The repository contains the Phase 0 foundation and the process model, scheduling, resource managers, controller, Qt dashboard, process controls, timeline, metrics, tests, and submission documentation described in Phases 1-9. The core suite is run with CTest. The Qt Widgets application has been built and startup-smoke-checked with Qt 6.8.3 and MinGW 13.1; the interactive visual acceptance checklist in `TESTING.md` still requires a desktop session.

## Work in small phases

Use `CODEX_PHASE_PROMPTS.md` and submit one phase per Codex request.
Keep the application buildable at each checkpoint.

## Standard loop for each phase

1.  **Inspect:** review the current tree, README, rules, and existing
    implementation.
2.  **Plan:** list the files to add/change and the acceptance criteria.
3.  **Implement:** change only what is required for the current phase.
4.  **Build:** configure/build using the project's documented commands.
5.  **Test:** run relevant unit tests and add regression tests.
6.  **Review:** check state transitions, memory release, queue
    duplication, edge cases, and UI responsiveness.
7.  **Report:** summarize changes, test commands/results, and
    limitations.

## Suggested Git checkpoints

Create a commit after each stable phase, for example: -
`phase-1: add process model and state machine` -
`phase-2: implement scheduling policies` -
`phase-3: add memory and io simulation` -
`phase-4: add simulation controller and metrics` -
`phase-5: build Qt dashboard and controls` -
`phase-6: add scenarios, tests, and documentation`

Do not commit secrets, generated build folders, IDE-specific private
files, or unrelated files.

## Definition of done for a phase

-   Project configures and compiles.
-   Tests for the new behavior pass.
-   No previously passing tests are broken.
-   UI actions, if added, have real behavior.
-   Documentation reflects the implementation.
-   Codex reports any limitations honestly.

## Manual integration checks

-   Start, pause, step, and reset work predictably.
-   A higher-priority real-time task preempts user work according to the
    documented policy.
-   MLFQ queue order, demotion, and priority boost are deterministic.
-   A blocked process does not consume CPU.
-   Suspended processes are never scheduled.
-   I/O completion correctly handles suspended-blocked processes.
-   Memory usage returns to baseline after all processes terminate.
-   Timeline and metrics agree with the event log.
-   Empty workloads and invalid inputs do not crash the application.

## Troubleshooting

-   If CMake cannot find Qt, configure the correct Qt kit/compiler or
    set `CMAKE_PREFIX_PATH` to the Qt installation.
-   If a component is unavailable, prefer the standard Qt Widgets
    alternative rather than adding a large dependency.
-   If tests expose a scheduler bug, fix the model/engine, not the
    displayed output.
-   If scope becomes too large, finish the core simulation before adding
    themes, animations, or persistence.
