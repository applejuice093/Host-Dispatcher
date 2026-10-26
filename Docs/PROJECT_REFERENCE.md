# HOST Dispatcher Simulator — Project Reference

This document is the end-to-end technical reference for the HOST Dispatcher Simulator. It explains the project goal, scheduling and resource rules, architecture, UI, public C++ API, project-defined function inventory, dependencies, build, packaging, testing, and known limits. It is written against the current source in `src/`; if the code changes, update this reference with it.

## 1. Project at a glance

HOST Dispatcher is an educational desktop simulator for operating-system process dispatch. It lets a user create User and Real-Time processes, advance a simulated clock, observe scheduling, and see simulated memory and I/O use. It does not dispatch real host processes or manage physical RAM or hardware.

The project implements the requirements in the project statement:

- Real-time scheduling with Earliest Deadline First (EDF).
- Multilevel Feedback Queue (MLFQ) scheduling for User processes.
- Admission controlled by a configurable simulated memory capacity.
- Per-device simulated I/O with active service and a FIFO waiting queue.
- Process admission, suspension, resumption, termination, and resource cleanup.

The simulation engine is independent of Qt. The Qt Widgets application is a client of the engine: it collects user input, calls `Simulation`, and renders the returned process/resource state and metrics.

## 2. What the application does

The dashboard provides:

- **Load Sample** to load four example processes (two Real-Time and two User), including Disk and Keyboard requests.
- **Add Process** to configure identity, type, arrival, CPU burst, memory, optional deadline, and optional one-shot I/O.
- **Set Capacity** to configure simulated memory before any process is added or the clock advances.
- **Step**, **Play**, and **Pause** to advance or stop the simulated clock. Play steps every 450 ms of wall-clock time; wall time does not determine CPU or I/O service duration.
- **Suspend**, **Resume**, and **Terminate** for the selected process, subject to its current state.
- **Reset** to clear the run after a confirmation prompt.
- A process table, MLFQ queues, memory accounting, I/O device requests, a CPU timeline, recent event messages, and metrics.

The table contains ID, name, type, state, arrival tick, original CPU burst, remaining CPU, queue, memory request, deadline, first start, completion, deadline-miss flag, and ready waiting time. The timeline retains the simulation history in the engine and draws the latest 40 ticks in the UI.

## 3. Technology stack and why it is used

| Technology | Use in this project | Reason |
|---|---|---|
| C++17 | Simulation engine and desktop application | Strongly typed, efficient, and suitable for modeling stateful scheduling logic. The core uses standard C++ and has no GUI dependency. |
| Qt 6 Widgets | Desktop window, controls, dialogs, table, timer, and custom timeline drawing | Provides a native desktop UI toolkit and event loop without requiring a web stack or a separate UI runtime. |
| CMake 3.21+ | Configure and build the core, tests, and optional GUI | Cross-platform build configuration and standard Qt integration. CMake still builds the core/tests when Qt Widgets is unavailable. |
| CTest | Runs the registered simulation test executable | Integrates the dependency-light core checks with the normal CMake workflow. |
| MinGW 13.1 + Qt 6.8.3 (verified Windows build) | Compiler and Qt runtime used for the packaged Windows build | Produces the Windows executable with the matching Qt/MinGW runtime. These are verified build versions, not minimum project requirements. |

The code also uses the C++ standard library: `std::map` for ordered process/device registries, `std::unordered_map` for allocation lookup, `std::deque` for FIFO queues, `std::array` for the fixed three MLFQ levels, `std::vector` for history/results, `std::string`/`std::ostringstream` for core text, algorithms for queue cleanup, numeric limits for overflow checks, and standard exceptions for invalid input/state. Qt types are confined to the UI layer.

There is no database, network service, persistence layer, third-party runtime library, or host-OS process-control API in the simulator.

## 4. Repository map

| Path | Responsibility |
|---|---|
| `CMakeLists.txt` | C++ standard, core library, optional Qt GUI target, and CTest target. |
| `src/simulation.h` | Core domain types and public engine API. |
| `src/simulation.cpp` | Validation, state transitions, schedulers, simulated resources, tick engine, and metrics. |
| `src/main.cpp` | Creates the Qt application and main window, then starts the event loop. |
| `src/main_window.h` | Qt timeline and main-window class declarations. |
| `src/main_window.cpp` | Builds the dashboard and connects UI actions to the simulation API. |
| `tests/simulation_tests.cpp` | Standalone assertion-based checks for the simulation core. |
| `Docs/ARCHITECTURE.md` | Short architecture and scheduling rules. |
| `Docs/USER_GUIDE.md` | Dashboard workflow and user instructions. |
| `Docs/TESTING.md` | Automated coverage and manual acceptance checklist. |
| `Docs/WORKFLOW.md` | Development workflow and project status. |
| `Docs/PROJECT_REFERENCE.md` | This full project, design, and API reference. |
| `release/host-dispatcher-windows-portable.zip` | Portable Windows application bundle, including the executable and runtime dependencies. |

Generated build folders, downloaded toolchains, `dist/`, and local logs are excluded from Git by `.gitignore`.

## 5. Architecture and data flow

```text
User actions
    |
Qt MainWindow ---------------------------> Simulation API
    |                                           |
TimelineWidget                         ProcessRegistry
    |                                    /      |      \
QPainter drawing                 HostDispatcher MemoryManager IoManager
                                       /      \
                                EDF scheduler  MLFQ scheduler
                                           |
                                      MetricsCollector
```

The engine owns its process registry, memory manager, I/O manager, scheduler instances, current running process, simulated clock, tick history, and messages. `MainWindow` owns one `Simulation`. The GUI does not implement scheduling policy; it calls the core and refreshes its controls from the core's observable state.

The most important boundary is `Simulation::step()`: each call performs one deterministic simulated tick, regardless of how long the machine takes to execute it. The Qt timer merely decides how often to call this function during Play.

## 6. Domain model and state machine

### Process types and states

`ProcessType` is `User` or `RealTime`. `ProcessState` is one of:

| State | Meaning |
|---|---|
| `New` | Configured but not admitted yet, usually waiting for its arrival tick or enough simulated memory. |
| `Ready` | Admitted and eligible for CPU scheduling. |
| `Running` | Currently using the simulated CPU. |
| `Blocked` | Waiting for its simulated I/O request to finish. |
| `SuspendedReady` | Ready work that the dispatcher must not schedule until resumed. |
| `SuspendedBlocked` | I/O-blocked work that remains suspended. If its I/O finishes, it becomes `SuspendedReady`. |
| `Terminated` | Finished normally or explicitly terminated; it cannot transition again. |

Allowed transitions are enforced internally by `canTransition` and `transition`:

| From | Allowed next states |
|---|---|
| New | Ready, Terminated |
| Ready | Running, SuspendedReady, Terminated |
| Running | Ready, Blocked, SuspendedReady, Terminated |
| Blocked | Ready, SuspendedBlocked, Terminated |
| SuspendedReady | Ready, Terminated |
| SuspendedBlocked | Blocked, SuspendedReady, Ready, Terminated |
| Terminated | none |

Suspension does not free memory or cancel I/O. Resuming a still-blocked task returns it to `Blocked`; if its I/O completed while suspended, it returns to `Ready`. Termination removes queued work/I/O and releases any memory reservation.

### Input data

- `IoRequest`: device name, CPU-work offset at which to issue the request, and service duration in simulated ticks.
- `ProcessSpec`: the user-provided process definition: ID, name, type, arrival tick, CPU burst, simulated memory amount, optional real-time deadline, and optional I/O request.
- `Process`: a `ProcessSpec` plus mutable runtime state and accounting fields: remaining CPU, MLFQ level and quantum use, ready waiting, CPU use, first start, completion, normal/explicit termination flags, I/O-issued flag, deadline-missed flag, and I/O-completed-while-suspended flag.
- `TickRecord`: a simulated tick and the process ID selected for it; ID `0` means the CPU was idle.
- `Metrics`: elapsed ticks, normal completions, real-time deadline counts/rate, average waiting/turnaround/response, CPU utilization, and throughput.

IDs must be unique positive integers. Names cannot be blank. Arrival must be nonnegative; CPU burst and memory request must be positive. Real-Time work requires a positive relative deadline, and User work cannot have one. Deadline addition is checked for integer overflow. An enabled I/O request needs a non-empty device, positive duration, and an offset strictly between zero and the CPU burst.

## 7. Scheduling and resource rules

### Real-time EDF

Only Ready Real-Time processes are eligible. The scheduler chooses the smallest absolute deadline (`arrival + relativeDeadline`); ties are resolved by earlier arrival and then lower process ID. A newly eligible Real-Time task preempts User work. A current Real-Time task is preempted by another Ready Real-Time task only if the candidate has higher EDF ordering.

Deadline misses are recorded once when an unfinished Real-Time process reaches or passes its absolute deadline. A task completing exactly at its deadline is on time because completion is processed before the end-of-tick deadline check.

### User MLFQ

The fixed queues are:

| Queue | Policy | Quantum |
|---|---|---:|
| Q0 | Round Robin | 2 ticks |
| Q1 | Round Robin | 4 ticks |
| Q2 | FCFS | No demotion quantum |

A full Q0/Q1 quantum demotes the process one level. A single I/O yield preserves its level and resets quantum use. A higher queue can preempt lower-queue User work. At Q2, work continues FCFS unless preempted by Real-Time work. Ready User tasks in Q1/Q2 are boosted to Q0 after 20 ready-wait ticks by default. `setPriorityBoostInterval(0)` disables boosts; another nonnegative value changes the interval.

Queue insertion removes any existing entries for the same process first, avoiding duplicate scheduling. Blocked and suspended tasks are not queued as Ready work.

### Simulated memory

Default capacity is 1024 MB. The UI permits changing it only before adding any process and before time advances; the API enforces the same precondition. Admission reserves the process's requested MB atomically. If it does not fit, it stays New and is reconsidered on later ticks. Memory is released at normal completion, explicit termination, or reset. This is accounting only; it never allocates machine RAM.

Admission scans the ordered process registry, which is keyed by ID. Therefore, when multiple due processes contend for scarce memory on the same tick, the lower-ID process is considered first.

### Simulated I/O

Each device name has at most one active request and a FIFO queue of waiting requests. Service and due times use the simulated clock. A process has at most one configured request in this version; duplicate outstanding requests for the same process are rejected. I/O starts after the process has consumed its configured CPU offset, then it blocks. Completion wakes a Blocked task or records completion for a SuspendedBlocked task. Terminating a process removes its active or waiting request; if it was active, the next queued request starts at the current simulated tick.

### Tick order

For `time == t`, `Simulation::step()`:

1. Admits due New processes if memory is available.
2. Completes I/O requests due at or before `t` and wakes eligible tasks.
3. Records deadline misses already due.
4. Selects/preempts work according to EDF precedence and MLFQ priority.
5. Executes one CPU unit or records an idle tick in `[t, t+1)`.
6. Handles completion, I/O blocking, or a User quantum boundary; releases resources on normal completion.
7. Advances the clock, increments Ready waiting for processes that did not run, and applies MLFQ boosts.
8. Checks deadline misses at the new boundary.

Every call adds exactly one `TickRecord`, including idle ticks.

## 8. Metrics definitions

- **Waiting time:** increments for each interval a process is Ready but not selected. Blocked and suspended time is excluded.
- **Turnaround:** completion tick minus arrival tick, for normally completed tasks.
- **Response:** first CPU start minus arrival tick, for normally completed tasks.
- **CPU utilization:** non-idle timeline records divided by elapsed ticks, as a percentage.
- **Throughput:** normally completed tasks divided by elapsed ticks.
- **Deadline miss rate:** missed Real-Time tasks divided by counted Real-Time tasks. A process explicitly terminated before missing its deadline is excluded from the denominator; tasks that already missed remain counted.
- **Averages:** calculated over normally completed processes. Empty denominators produce zero rather than division by zero.

Explicit termination releases resources but does not count as normal completion. The metrics are derived from process records and the tick timeline, not estimated from UI activity.

## 9. Public C++ API reference

All core declarations are in `src/simulation.h` under namespace `host_dispatcher`. The API is synchronous and deterministic. Unless stated otherwise, methods that reject invalid configuration throw `std::invalid_argument`; impossible state changes can throw `std::logic_error`. Map access through `at()` follows `std::map::at` and throws `std::out_of_range` for unknown IDs.

### Types and formatting helpers

| Symbol | API / behavior |
|---|---|
| `ProcessType` | `User`, `RealTime`. Selects scheduling policy. |
| `ProcessState` | The seven states described above. |
| `IoRequest` | Aggregate fields: `device`, `afterCpu`, `duration`. |
| `ProcessSpec` | Aggregate input record with all process configuration fields. Defaults describe a minimal User process with ID 0 (which validation rejects until assigned), one CPU tick, and 1 MB. |
| `Process` | Runtime record. `absoluteDeadline() const` returns arrival plus relative deadline when enabled, otherwise `-1`. |
| `toString(ProcessType)` | Returns `"User"` or `"Real-Time"`. |
| `toString(ProcessState)` | Returns a display string for each process state. |
| `TickRecord` | `{time, processId}` entry; process ID 0 means idle. |
| `Metrics` | Result record with fields defined in Section 8. |

### `ProcessRegistry`

| Method | Behavior |
|---|---|
| `add(const ProcessSpec&)` | Validates the spec, creates a `Process` in New state with remaining CPU initialized to the burst, then inserts by ID. Rejects invalid fields and duplicate IDs. |
| `clear()` | Removes all process records. |
| `contains(int) const` | Checks whether an ID exists. |
| `at(int)` / `at(int) const` | Returns mutable/const process access for an existing ID. |
| `all() const` | Returns the const ordered map of ID to process. |

### `MemoryManager`

| Method | Behavior |
|---|---|
| `MemoryManager(int capacityMb = 1024)` | Creates a manager; capacity must be positive. |
| `allocate(int id, int amountMb)` | Atomically reserves amount for a positive, not-yet-allocated ID if it fits; returns false on invalid ID/amount, duplicate allocation, or insufficient free space. |
| `release(int id)` | Frees that process's reservation; returns false if there was none. |
| `clear()` | Removes all allocations and resets usage to zero while retaining capacity. |
| `capacity()`, `used()`, `free()` | Read-only integer accounting accessors. |

### `IoManager`

| Method | Behavior |
|---|---|
| `request(device, processId, now, duration)` | Validates request parameters and uniqueness of the process's outstanding request, then starts it or appends it to that device's FIFO. Throws `std::invalid_argument` for invalid/duplicate requests. |
| `completeDue(now)` | Completes active requests whose due time is at or before `now`; returns their process IDs and starts the next queued request for each device. |
| `remove(processId, now)` | Removes the process from active/waiting I/O. If active, starts the next waiting request at `now`. |
| `clear()` | Removes all devices and requests. |
| `describe() const` | Formats active and waiting requests for the dashboard, or returns `"No active I/O requests"`. |

Nested `IoManager::Request` stores process ID, due time, and duration. Internal `Device` stores one active request and a FIFO deque.

### Schedulers and dispatcher

| Class / method | Behavior |
|---|---|
| `RealTimeScheduler::selectEarliestReady(const ProcessRegistry&) const` | Returns the selected Ready Real-Time ID by EDF/tie-break ordering; returns 0 if none is ready. |
| `MLFQScheduler::enqueue(Process&, bool front = false)` | Inserts an eligible Ready User process at the back or front of its level queue after removing old copies. |
| `MLFQScheduler::remove(int id)` | Erases an ID from all MLFQ queues. |
| `MLFQScheduler::selectNext(const ProcessRegistry&)` | Pops stale entries until it finds a Ready User process; selects the first queue with work or returns 0. |
| `MLFQScheduler::hasReadyAbove(int level) const` | Reports whether any queue above the supplied level has an entry. |
| `MLFQScheduler::clear()` | Empties all three queues. |
| `MLFQScheduler::readyQueues() const` | Returns a snapshot as three vectors of process IDs, in queue order. |
| `HostDispatcher::select(processes, EDF, MLFQ, currentId) const` | Applies preemption policy, maintains/requeues the current process where needed, and selects EDF work before MLFQ work. Returns 0 for idle. |

### Metrics and simulation controller

| Method | Behavior |
|---|---|
| `MetricsCollector::collect(processes, timeline, elapsed) const` | Derives utilization, completion counts, averages, throughput, and deadline counts/rate from engine records. |
| `Simulation(int memoryMb = 1024)` | Creates an empty simulation and its managers. Invalid capacity is rejected by `MemoryManager`. |
| `addProcess(const ProcessSpec&)` | Validates/registers a process. It does not admit the process until its arrival is due during a step. |
| `loadSample()` | Resets the current engine, then adds the four built-in sample processes. |
| `setMemoryCapacity(int)` | Changes capacity only before processes exist and before time advances; otherwise throws `std::logic_error`. Capacity itself must be positive. |
| `setPriorityBoostInterval(int)` | Sets a nonnegative Ready-wait interval; zero disables boosts, negative values throw `std::invalid_argument`. |
| `step()` | Runs one deterministic tick, performs admission, I/O, dispatch, CPU accounting, resource changes, queue aging, and deadline checks. |
| `run(int ticks)` | Calls `step()` the requested number of times; negative counts throw `std::invalid_argument`. |
| `suspend(int id)` | Suspends Ready, Running, or Blocked work and removes it from MLFQ queues; returns false for missing IDs or unsupported states. Memory and I/O are retained. |
| `resume(int id)` | Resumes SuspendedReady or SuspendedBlocked work, preserving the distinction between pending and completed I/O; returns false for missing IDs or other states. |
| `terminate(int id)` | Marks any nonterminated existing process as explicitly terminated, records completion boundary if absent, releases memory and I/O, and removes queued/current state. Returns false if missing/already terminated. |
| `reset()` | Clears processes, allocations, I/O, clock, running ID, ready queues, timeline, and messages. Policy configuration such as boost interval remains set. |
| `processes()`, `memory()`, `io()` | Return const views of the process registry and resource managers. |
| `timeline()`, `messages()` | Return const references to history and event text. |
| `time() const` | Returns current simulated tick. |
| `metrics() const` | Returns a freshly computed `Metrics` value. |
| `readyQueues() const` | Returns the MLFQ queue snapshot. |

### Core private helpers (implementation map)

These are not external API, but explain how `Simulation` is divided internally:

| Helper | Responsibility |
|---|---|
| `canTransition(from, to)` | Central legal-transition table. |
| `transition(process, next)` | Checks the table, changes state, and throws `std::logic_error` for illegal transitions. |
| `Simulation::enqueue(process, front)` | Keeps the MLFQ entry synchronized for a Ready User task. |
| `removeFromQueues(id)` | Removes a process from all MLFQ queues. |
| `chooseNext()` | Delegates dispatch selection to `HostDispatcher`. |
| `admitArrivals()` | Admits due New processes and reserves memory. |
| `wakeIo()` | Converts completed I/O into Ready/SuspendedReady state as appropriate. |
| `updateDeadlineMisses()` | Marks each newly overdue unfinished Real-Time process once. |
| `ageReadyProcesses(exceptId)` | Adds Ready waiting and applies configured MLFQ boosts. |
| `IoManager::startNext(device, now)` | Activates the next FIFO request with a due time based on its service duration. |

## 10. Qt UI class and function reference

Qt headers are included only by `src/main_window.h/.cpp` and `src/main.cpp`.

### `main()`

`main(int argc, char *argv[])` constructs `QApplication`, constructs and shows `MainWindow`, and returns `application.exec()`. `QApplication` owns the desktop event loop that delivers button, selection, paint, and timer events.

### `TimelineWidget`

| Method | Behavior |
|---|---|
| `TimelineWidget(QWidget *parent)` | Sets minimum timeline height and expanding horizontal size policy. |
| `setRecords(const std::vector<TickRecord>&)` | Copies current history and schedules a repaint. |
| `paintEvent(QPaintEvent*)` | Paints a dark background, idle/process colored tick bars, process IDs, and tick labels for the latest 40 records; shows an empty-state message when no records exist. |

### `MainWindow`

| Method/slot | Behavior |
|---|---|
| `MainWindow(QWidget *parent)` | Builds the toolbar and Simulation tab, creates the engine, connects actions, configures the 450 ms timer, and performs initial refresh. |
| `makeDashboard()` | Creates the summary label, 14-column process table, timeline, queue/memory/I/O panels, metrics label, and recent-message view. |
| `refresh()` | Reads a consistent engine snapshot into all visible widgets while preserving row selection where possible; also updates which actions are enabled. |
| `selectedProcessId() const` | Converts the selected table row's ID to an integer, or returns 0 when no row is selected. |
| `updateActionAvailability()` | Enables Suspend/Resume/Terminate according to the selected process's state. |
| `addProcess()` | Displays the modal process form, maps fields to `ProcessSpec`, calls `Simulation::addProcess`, and displays validation errors. |
| `togglePlay()` | Starts/stops the `QTimer` and changes the button caption between Play and Pause. |
| `suspendSelected()` | Calls `Simulation::suspend` for the selected ID, displays a state hint if rejected, and refreshes. |
| `resumeSelected()` | Calls `Simulation::resume`, displays a state hint if rejected, and refreshes. |
| `terminateSelected()` | Calls `Simulation::terminate`, displays a state hint if rejected, and refreshes. |

### UI event connections

- Load Sample stops Play, resets/loads the scenario, and refreshes.
- Add Process opens the modal form.
- Set Capacity applies the spin-box value; it catches errors and uses a warning dialog.
- Step advances once and refreshes.
- Reset asks for confirmation if there is existing work/time, then stops Play, resets, and refreshes.
- Play/Pause uses `togglePlay`.
- Timer timeout calls `step()` then `refresh()`.
- Process table selection changes update action availability.
- The process-type combo enables the deadline field only for Real-Time tasks. The I/O checkbox enables the device/offset/duration controls only when checked.

## 11. External API surface used

This section names the framework entry points and API families the project relies on; the project-defined function inventory above is exhaustive for functions declared in this repository.

### Qt 6 Widgets APIs

- **Application/window lifecycle:** `QApplication` construction and `exec()` start the event loop. `QMainWindow`/`QWidget` usage calls `setWindowTitle`, `setMinimumSize`, `resize`, `setCentralWidget`, and `show`.
- **Layouts:** `QVBoxLayout`, `QHBoxLayout`, and `QFormLayout` use `addWidget`, `addLayout`, `addStretch`, and `addRow` to place controls.
- **Labels and basic controls:** `QLabel` uses `setText`, `setWordWrap`, `setAlignment`, and `setTextInteractionFlags`; `QPushButton` uses `setText` and `setEnabled`; `QSpinBox` uses `setRange`, `setValue`, `setSuffix`, `value`, `blockSignals`, and `setEnabled`; `QLineEdit` uses `setText`/`text`; `QComboBox` uses `addItems`, `currentIndex`, and `currentText`; `QCheckBox` uses `isChecked`.
- **Dashboard controls:** `QTabWidget::addTab`; `QTableWidget` uses its constructor plus `setHorizontalHeaderLabels`, `setSelectionBehavior`, `setSelectionMode`, `setEditTriggers`, `horizontalHeader`, `setRowCount`, `setItem`, `rowCount`, `currentRow`, `item`, and `selectRow`; `QHeaderView::setSectionResizeMode`; `QTableWidgetItem::text`; `QPlainTextEdit::setReadOnly`, `setMaximumHeight`, `setPlaceholderText`, and `setPlainText`.
- **Signals and slots:** `QObject::connect` connects `QPushButton::clicked`, `QTableWidget::itemSelectionChanged`, `QComboBox::currentIndexChanged`, `QCheckBox::toggled`, and `QTimer::timeout`; `qOverload<int>` selects the integer combo signal overload. The application uses both member slots and lambdas.
- **Dialogs:** `QDialog::exec` returns acceptance/cancellation. `QDialogButtonBox` supplies accepted/rejected signals. `QMessageBox::warning`, `information`, and `question` report errors, invalid actions, and reset confirmation.
- **Timer:** `QTimer::setInterval`, `start`, `stop`, and `isActive` provide wall-clock pacing of the deterministic engine.
- **Custom timeline drawing:** `QWidget::update` schedules repaint; `QWidget::rect`/`width` provide geometry; `QPainter` calls `fillRect`, `setRenderHint`, `setPen`, and `drawText`; `QColor` construction and `fromHsv` define colors; `QSizePolicy` and `QWidget::setMinimumHeight`/`setSizePolicy` define sizing. `paintEvent` is the Qt paint override.
- **Text and values:** `QStringLiteral`, `QString::arg`, `number`, `fromStdString`, `fromUtf8`, `trimmed`, `toStdString`, and `toInt`, plus `QStringList::join`, bridge and format values between the core and widgets.

### CMake/CTest APIs

The project uses `cmake_minimum_required`, `project`, `set`, `option`, `if`/`else`/`endif`, and `message` to define project metadata, the C++17 requirement, optional GUI switch, and missing-Qt notice. `add_library` and `target_include_directories` define the core. `find_package(Qt6 6.2 QUIET COMPONENTS Widgets)` discovers the optional desktop dependency. `CMAKE_AUTOMOC` enables Qt's meta-object compiler for `Q_OBJECT`; `qt_add_executable` defines the GUI target; `target_link_libraries` connects the app to the core and Qt Widgets. `enable_testing`, `add_executable`, and `add_test` register the core test executable with CTest. If Qt is absent, the core and test executable still build.

### C++ standard library API families

- `std::map`/`std::unordered_map`: `count`, `emplace`, `find`, `erase`, `clear`, `at`, `begin`/`end`, `rbegin`, `empty`, and keyed iteration support registries and allocation accounting.
- `std::deque`: `empty`, `front`, `push_front`, `push_back`, `pop_front`, `erase`, and iteration implement the ready and I/O queues.
- `std::array`/`std::vector`: indexing, `size`, `empty`, `assign`, `push_back`, `front`, `back`, and iteration store fixed scheduling levels and chronological records/results.
- `std::string`/`std::ostringstream`: `empty`, `find_first_not_of`, concatenation, stream insertion, and `str()` validate and format core messages.
- `<algorithm>`: `std::remove`, `std::remove_if`, and `std::max` remove queue entries and clamp response-time accounting.
- `<limits>`: `std::numeric_limits<int>::max()` guards deadline and I/O due-time arithmetic against overflow.
- `<stdexcept>`: `std::invalid_argument`, `std::logic_error`, `std::out_of_range` (through checked container access), and `std::runtime_error` (test helper) communicate failures.
- Test output uses `std::cout`, `std::cerr`, `std::to_string`, and the `EXIT_SUCCESS`/`EXIT_FAILURE` constants; `std::exception::what()` supplies diagnostics.

## 12. Build, run, test, and package

### Prerequisites

- CMake 3.21 or later.
- A C++17 compiler.
- Qt 6 Widgets 6.2 or later for the GUI. It is optional for building only the core and tests.

### Configure and build

From the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

When CMake cannot find Qt, set `CMAKE_PREFIX_PATH` to the Qt installation for the compiler being used, then reconfigure. To disable GUI discovery explicitly, configure with `-DHOST_DISPATCHER_BUILD_GUI=OFF`.

### Windows portable package

The committed portable bundle is `release/host-dispatcher-windows-portable.zip`. Extract the entire ZIP and run `host-dispatcher.exe` from the extracted folder. Keep the Qt DLLs, MinGW runtime DLLs, and plugin directories beside the executable. The EXE by itself is not a self-contained single-file program. `windeployqt --release --no-translations --compiler-runtime` is the Qt deployment step used to assemble those dependencies.

## 13. Tests and evidence

`tests/simulation_tests.cpp` is a small standalone C++ executable with named checks. It covers process validation, state transitions, EDF ordering and tie breaks, Real-Time preemption, Q0/Q1 demotion, priority boosts and unique queue membership, memory accounting and capacity rules, I/O serialization and duplicate requests, blocked/suspended I/O, deadline behavior, metrics, admission retry after memory release, termination cleanup, idle ticks, and reset. CTest invokes this executable as `simulation_tests`.

The test harness has three project-defined functions:

| Function | Behavior |
|---|---|
| `check(bool condition, const std::string& message)` | Increments the assertion count and throws `std::runtime_error(message)` when a condition is false. |
| `task(int id, ProcessType type, int burst, int arrival = 0, int deadline = 0)` | Builds a standard test `ProcessSpec`; for Real-Time inputs it also enables the deadline and sets the relative deadline. |
| `main()` | Runs the checks, prints the passing assertion count, and reports the first thrown test failure to stderr with a failure exit status. |

The documented build environment has Qt 6.8.3 and MinGW 13.1. The core suite previously reported 36 passing simulation checks, and an offscreen GUI startup smoke check was performed. Automated core tests do not replace interactive visual checks; the manual list in `Docs/TESTING.md` remains the UI acceptance guide.

## 14. Scope, limitations, and extension points

Current limitations:

- One simulated CPU.
- One optional I/O request per process.
- One active request per device, with a FIFO waiting queue.
- Aggregate simulated memory capacity only; no paging, segmentation, swapping, or physical allocation.
- No persistent scenarios, save/load, report export, multi-core scheduling, or Qt UI automation.
- No interaction with real host processes, hardware devices, or actual memory.
- The priority-boost interval is exposed by the core API but is not currently configurable in the UI.

Natural extension points are `Simulation` for new event/lifecycle behavior; `RealTimeScheduler`, `MLFQScheduler`, and `HostDispatcher` for policy changes; resource managers for additional simulated resources; `MetricsCollector` for new reported measurements; and `MainWindow` for controls/visualizations. Preserve state-transition validation and resource cleanup when extending lifecycle operations.

## 15. Glossary

- **Admission:** Moving an arrived New process to Ready after simulated memory is reserved.
- **EDF:** Earliest Deadline First, the Real-Time scheduling order.
- **MLFQ:** Multilevel Feedback Queue, three priority queues used for User work.
- **Quantum:** Maximum consecutive CPU ticks allowed at a round-robin level before demotion.
- **Ready wait:** Time eligible for CPU but not selected; it contributes to waiting metrics and boost age.
- **Suspended:** Temporarily ineligible for dispatch; resources remain reserved.
- **Tick:** One unit of simulated time, with exactly one CPU selection or idle record.

## 16. Presentation and viva preparation

### Short project explanation

> HOST Dispatcher is a C++17 and Qt 6 desktop simulator for process scheduling and resource management. It schedules Real-Time tasks with EDF and User tasks with a three-level MLFQ, and simulates memory admission and per-device I/O queues. Users can add, suspend, resume, terminate, step, and reset processes while viewing states, resource use, a CPU timeline, and metrics. The engine uses its own deterministic clock and never controls real operating-system processes or hardware.

### Suggested demonstration

1. Start with the empty dashboard and explain that one **Step** is one simulated tick.
2. Choose **Load Sample**. Point out the four process rows, their types, arrivals, deadlines, memory requests, and configured I/O.
3. Step through the first ticks. Show Real-Time work preempting User work and explain that EDF compares absolute deadlines.
4. Continue until a User task consumes its Q0 quantum. Show the queue level change and explain Q0/Q1 round-robin quanta and Q2 FCFS.
5. Continue until I/O occurs. Show the task as Blocked, the active device request, and its return to Ready after simulated service completes.
6. Select a task and demonstrate Suspend and Resume. Mention that a suspended task retains its memory; a suspended blocked task becomes Suspended Ready if I/O completes before it resumes.
7. Terminate a different task and show memory use decrease and its I/O request disappear if one was pending.
8. Point out the timeline and metrics, then use Reset to clear the run.

The exact trace depends on the order of the user's actions. Explain the policy and observe the UI state instead of memorizing one timeline.

### Likely questions and concise answers

| Question | Answer |
|---|---|
| What is the goal of the project? | To demonstrate how an operating-system dispatcher can schedule Real-Time and User work while tracking simulated memory and I/O resources. |
| Is this a real process manager? | No. All processes, time, memory, and devices are modeled inside the application. It does not control Windows/Linux/macOS processes or physical hardware. |
| Why use EDF for Real-Time tasks? | EDF directly orders ready tasks by the nearest absolute deadline, so the dispatcher can prioritize the most time-urgent eligible task. |
| Why use MLFQ for User tasks? | It gives short/interactive work a high-priority queue while moving CPU-heavy work to lower queues. The policy is easy to demonstrate through quanta and demotion. |
| How do the two schedulers interact? | Any eligible Real-Time task takes precedence over User queues. EDF orders Real-Time tasks; MLFQ orders User tasks. A more urgent Ready Real-Time task can preempt the current task. |
| How does admission work? | A New process waits until its arrival tick. The engine allocates its requested simulated memory atomically if enough capacity is free; otherwise it stays New and is retried on later ticks. |
| What happens when a process requests I/O? | After it uses the configured amount of CPU, it blocks. Its device request runs for its service duration in simulated ticks. Each device serves one request at a time and queues the rest FIFO. |
| What happens if I/O completes while a process is suspended? | It moves from Suspended Blocked to Suspended Ready and remains unschedulable until resumed. |
| When are resources released? | Normal CPU completion and explicit termination release simulated memory. Termination also removes active/queued I/O and ready-queue entries. Reset clears all simulation resources. |
| How is one tick implemented? | `Simulation::step()` admits arrivals, completes due I/O, checks deadlines, dispatches and executes at most one CPU unit, handles completion/I/O/quantum effects, advances time, updates waiting, and checks deadlines again. |
| What does the 450 ms timer mean? | It is only the GUI's real-time delay between calls to `step()` during Play. CPU and I/O durations are measured by simulated ticks. |
| What do the metrics mean? | Waiting counts Ready time not selected; turnaround is completion minus arrival; response is first start minus arrival; utilization is busy ticks over elapsed ticks; throughput is normal completions over elapsed ticks. |
| Why C++17 and Qt Widgets? | C++ provides a clear, efficient stateful simulation core; Qt Widgets supplies a desktop event loop, forms, tables, dialogs, and drawing without mixing UI code into scheduling logic. |
| How is correctness checked? | The standalone core test executable covers validation, EDF/MLFQ order, preemption, memory, I/O, suspension, deadlines, metrics, cleanup, and reset. The manual checklist covers the interactive GUI. |
| What are the main limitations? | One CPU, one optional I/O request per process, one active request per device, aggregate simulated memory only, and no persistence, exports, multiprocessor scheduling, or real OS integration. |
| What would you improve next? | Add editable boost settings in the UI, save/load scenarios, richer resource models, report export, multi-core policies, and automated GUI acceptance checks while preserving a separate deterministic core. |

These are preparation notes, not substitutes for understanding the code. For a question about a specific method, follow the API/function tables above to the source file and explain the behavior and the reason for it.
