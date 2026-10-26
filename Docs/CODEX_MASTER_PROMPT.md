# Codex Master Prompt

You are the implementation agent for **G-9: Integrated HOST Dispatcher
with Real-Time and User Process Scheduling**.

Read `RULES.md`, `TECH_STACK.md`, `ARCHITECTURE.md`, `WORKFLOW.md`, and
`TESTING.md` before coding. Build a C++17/Qt 6 Widgets/CMake desktop
**simulation** of an OS dispatcher. Simulate real-time EDF scheduling,
three-level MLFQ for user processes, process
admission/suspension/resumption/termination, memory allocation/release,
I/O blocking/completion, and performance metrics. Do not modify the
actual operating system or claim to manage real processes/hardware.

Work strictly one phase at a time using `CODEX_PHASE_PROMPTS.md`. First
inspect the repository and complete Phase 0. Do not silently skip phases
or implement the entire project in one response. Keep the app buildable
and tests passing after each phase. Separate simulation/domain logic
from the Qt UI. Add deterministic tests for scheduling, state
transitions, memory, I/O, metrics, and edge cases. Use clear,
maintainable C++ and explain assumptions. Never leave fake metrics,
inert controls, or unlabelled placeholders.

At the end of every phase, report: - summary of changes, - files
created/modified, - build and test commands run, - test results, - known
limitations, - the exact next phase to run.

If a requirement is ambiguous, choose the simplest defensible
educational simulation behavior, document it, and keep it configurable
when reasonable. Do not ask questions for minor decisions that can be
documented. Do ask before making a major scope change.
