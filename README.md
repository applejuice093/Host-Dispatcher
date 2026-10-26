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

## Build

Requirements: CMake 3.21+, a C++17 compiler, and Qt 6 Widgets 6.2+ for the desktop UI. If Qt is unavailable, CMake still builds the simulation core and tests and reports that it skipped the GUI.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

If the GUI is skipped, set `CMAKE_PREFIX_PATH` to a Qt 6 installation compatible with your compiler and reconfigure. You can disable the GUI explicitly with `-DHOST_DISPATCHER_BUILD_GUI=OFF`.

## Dependencies and licensing

The source uses the C++ standard library and Qt 6 Widgets; no third-party source is bundled. Qt is provided by the developer's Qt installation, so review the terms for that Qt distribution when redistributing a built application. No project-specific license has been selected for this repository.

## Try it

Launch `host-dispatcher`, choose **Load Sample**, then use **Step** or **Play**. Select a process row to suspend, resume, or terminate it. **Add Process** accepts user and real-time tasks and optional simulated I/O. Reset clears the entire simulation after confirmation.

## Windows portable build

Download and extract [the Windows portable bundle](release/host-dispatcher-windows-portable.zip), then run `host-dispatcher.exe` from the extracted folder. Keep the bundled DLLs and plugin folders beside the executable.

## Documentation

See [Docs/README.md](Docs/README.md) for the project documentation map, [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md) for scheduling and timing rules, and [Docs/USER_GUIDE.md](Docs/USER_GUIDE.md) for the demonstration flow.
