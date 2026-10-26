# Technology Stack

## Selected stack

  -----------------------------------------------------------------------
  Area                    Choice                  Purpose
  ----------------------- ----------------------- -----------------------
  Language                C++17                   Domain logic and
                                                  simulation

  GUI                     Qt 6 Widgets            Desktop UI, tables,
                                                  controls, dialogs

  Build                   CMake                   Portable project
                                                  configuration

  Testing                 Qt Test, optionally     Deterministic unit and
                          plain C++ tests         integration tests

  Data                    In-memory model + JSON  Reliable demo and
                          scenarios               repeatable test cases

  Charts                  Custom QWidget/QPainter Gantt timeline and
                          or Qt Charts if         usage visualization
                          installed               

  IDE                     Qt Creator or VS Code   Development
                          with Qt/CMake tools     
  -----------------------------------------------------------------------

## Why Qt Widgets?

The project needs tables, controls, dialogs, queue views, and a
timeline. Qt Widgets is suitable for a student desktop application and
integrates with CMake. Keep the core simulation independent from the GUI
so it can be tested without launching the window.

## Build requirements

-   Qt 6 installation including Qt Widgets.
-   CMake 3.21 or newer (the exact minimum may be adjusted to the
    installed Qt version).
-   A C++17 compiler supported by the selected Qt version.
-   Ninja or a platform-native build tool, optional but recommended.
-   Qt Test component for tests.

## Suggested CMake configuration

Use `find_package(Qt6 REQUIRED COMPONENTS Widgets)` for the application.
Add `Test` only if available and needed for test targets. Enable
`CMAKE_AUTOMOC`, `CMAKE_AUTOUIC`, and `CMAKE_AUTORCC` only as required
by the project.

## Dependencies policy

Prefer Qt and the C++ standard library only. Do not add a charting
dependency solely for one chart; a custom painted Gantt widget is
acceptable. If Qt Charts is chosen, verify that the user's Qt
installation includes it and provide a graceful build/configuration
note.

## Runtime model

This application is a simulator: - It does not install a kernel
module. - It does not change host process priorities. - It does not
allocate physical RAM to simulated processes. - It does not control
actual disk, keyboard, or printer devices. - A `QTimer` may drive
simulated ticks, but simulated time must remain separate from wall-clock
time.

## Suggested build commands

Configure and build from the repository root:

``` bash
cmake -S . -B build
cmake --build build
```

Run tests if configured:

``` bash
ctest --test-dir build --output-on-failure
```

The exact Qt kit/compiler setup depends on the developer's installed
environment.
