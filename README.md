# HOST Dispatcher Simulator

An educational C++17 and Qt 6 Widgets application for simulating OS dispatching. It models processes, CPU time, memory, and I/O inside the application. It does not control host processes, physical memory, or hardware.

## Features

- Validated process creation and explicit process states, including suspension.
- EDF scheduling for real-time tasks, with real-time work taking precedence over user work.
- Three-level MLFQ for user tasks: Q0 RR 2, Q1 RR 4, Q2 FCFS, with configurable priority boosts.
- Configurable simulated admission memory, one active and FIFO queued I/O request per device, and cleanup on termination.
- Deterministic one-tick stepping, Play/Pause, reset, sample scenario, process actions, and a CPU timeline.
- Waiting, turnaround, response, utilization, throughput, completion, and deadline-miss count/rate metrics.

The first version supports one CPU and one configured I/O request per process. It does not include persistence, report export, or multiple CPUs.

## Project explanation

HOST Dispatcher is a teaching tool for seeing how an operating system could choose which process gets the CPU while keeping track of memory and I/O. It runs a model of these resources inside the application; it does not change the priority of real programs, use physical RAM as simulated memory, or operate actual devices.

Each process is configured with a type, arrival time, CPU work, and memory request. A Real-Time process also has a deadline and is scheduled using Earliest Deadline First (EDF). User processes use a three-level Multilevel Feedback Queue (MLFQ): short CPU bursts begin in the highest-priority queue, and longer-running work moves to lower queues when it uses its time quantum. Ready Real-Time work takes precedence over User work.

When a process arrives, the simulator admits it only if enough simulated memory is available. A process can issue one optional I/O request after using part of its CPU burst. It then blocks while its named device serves the request; each device handles one request at a time and keeps additional requests in FIFO order. The process returns to the ready state when service completes. Users can also suspend, resume, or terminate processes. Completion and termination release simulated memory, and termination removes outstanding simulated I/O.

The simulation advances in discrete ticks. One **Step** advances one tick; **Play** repeatedly advances ticks using a GUI timer. At every tick the engine handles arrivals and I/O, chooses work with EDF/MLFQ, updates process state and resources, and records the CPU timeline. The dashboard derives metrics from those records and shows states, queues, resources, and deadline misses so the scheduling decisions can be inspected.

The engine is written in standard C++17 and is separate from the Qt 6 Widgets interface. This keeps scheduling and resource rules usable and testable without launching the desktop UI. See [Docs/PROJECT_REFERENCE.md](Docs/PROJECT_REFERENCE.md) for the full architecture, function/API reference, algorithm details, testing notes, and likely presentation questions.

## Build

Requirements: CMake 3.21+, a C++17 compiler, and Qt 6 Widgets 6.2+ for the desktop UI. If Qt is unavailable, CMake still builds the simulation core and tests and reports that it skipped the GUI.

After cloning, install those prerequisites, open a terminal in the repository, and run this single command to configure, build, and launch the GUI. On Windows, it also deploys the Qt and compiler runtime files beside the app before launching:

```sh
cmake -P scripts/run.cmake
```

The script works with single- and multi-configuration CMake generators. On Windows it requires Qt's `windeployqt` deployment tool. If Qt is installed in a nonstandard location, set `CMAKE_PREFIX_PATH` to its Qt installation before running the command.

### Build and test manually

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Run only the engine tests (no Qt needed)

The scheduling engine is plain C++17, so you can check it on any machine with CMake and a compiler:

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

This builds into `build-core/` with the GUI turned off and compiler warnings on (`-DHOST_DISPATCHER_WARNINGS=OFF` turns them off).

If the GUI is skipped, set `CMAKE_PREFIX_PATH` to a Qt 6 installation compatible with your compiler and reconfigure. You can disable the GUI explicitly with `-DHOST_DISPATCHER_BUILD_GUI=OFF`.

### Troubleshooting

- **"Qt 6 Widgets was not found"**: CMake could not see Qt. Point it at your Qt kit, for example `cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64`, and make sure the kit matches your compiler (MinGW Qt with MinGW, MSVC Qt with MSVC).
- **The Windows app closes immediately or reports missing DLLs**: run it from a folder where `windeployqt` has copied the Qt runtime, which `cmake -P scripts/run.cmake` does for you.
- **A process stays in New forever**: check the event log. Either memory is still held by other processes, or the process asks for more memory than the whole simulated capacity, which the log reports as "can never be admitted".

## Dependencies and licensing

The source uses the C++ standard library and Qt 6 Widgets; no third-party source is bundled. Qt is provided by the developer's Qt installation, so review the terms for that Qt distribution when redistributing a built application. No project-specific license has been selected for this repository.

## Try it

Launch `host-dispatcher`, choose **Load Sample**, then use **Step** or **Play**. Select a process row to suspend, resume, or terminate it. **Add Process** accepts user and real-time tasks and optional simulated I/O. Reset clears the entire simulation after confirmation.

## Windows portable build

Download and extract [the Windows portable bundle](release/host-dispatcher-windows-portable.zip), then run `host-dispatcher.exe` from the extracted folder. Keep the bundled DLLs and plugin folders beside the executable.

## Documentation

Start with the complete [project reference](Docs/PROJECT_REFERENCE.md), which covers the architecture, algorithms, every project-defined API/function, technology choices, build/package steps, and limitations. See [Docs/README.md](Docs/README.md) for the documentation map, [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md) for scheduling and timing rules, and [Docs/USER_GUIDE.md](Docs/USER_GUIDE.md) for the demonstration flow.
