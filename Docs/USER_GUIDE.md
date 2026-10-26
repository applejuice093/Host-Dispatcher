# User Guide

## Build and launch

Install CMake 3.21+, a C++17 compiler, and Qt 6 Widgets 6.2+ for the desktop application. From the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the generated `host-dispatcher` executable from the build directory. Without Qt, CMake can build and test the simulation core but will skip the GUI target.

## Run a sample

Choose **Load Sample** to load a real-time sensor, interactive user task, batch task, and a second real-time task. The scenario includes simulated Disk and Keyboard I/O. Use **Step** to advance one tick or **Play** to advance every 450 ms of wall-clock time. Wall-clock time only controls how often the simulator steps; all scheduling and I/O use the simulated clock. **Pause** stops advancement. **Reset** confirms, then clears processes and resources.

Before adding processes or advancing time, use the **Memory** field and **Set Capacity** to choose simulated memory capacity. After a run begins, capacity is locked until Reset.

## Add a simulated process

Choose **Add Process**, enter a unique positive ID, name, arrival tick, positive CPU burst, and memory request. For a real-time task, add a positive relative deadline; the UI derives its absolute deadline as arrival plus relative deadline. Optionally enable one simulated I/O request and choose a device, CPU offset, and service duration. The request offset must occur before CPU completion.

Due processes are admitted when their arrival time is reached if enough simulated memory is available. If memory is insufficient, the process remains New and admission is retried after memory is released. No partial memory allocation occurs.

## Manage processes

Select a row in the process table. **Suspend** accepts Ready, Running, and Blocked tasks; **Resume** accepts suspended tasks; **Terminate** accepts any task that has not terminated. Invalid actions show a short explanation. Termination removes simulated queue/device requests and releases memory. Only normal CPU completion counts as successful completion in the report.

## Read the dashboard

- The table lists identity, type, state, remaining CPU, queue level, memory request, deadline, and Ready waiting.
- The CPU event history includes an entry for every tick; the timeline displays the latest 40 ticks, with idle slots labelled Idle.
- MLFQ queue order, memory capacity/use/free, and active/queued device requests appear below the timeline.
- The metrics line shows normal completions, average waiting/turnaround/response, utilization, throughput, and real-time deadline miss count/rate.
- Recent admission and deadline events appear at the bottom.

Scheduling uses real-time EDF ahead of user work, then MLFQ with Q0 RR 2, Q1 RR 4, and Q2 FCFS. A full quantum demotes a user task one level. One I/O yield retains its current level. Waiting user tasks at Q1 or Q2 receive a priority boost after 20 Ready ticks by default.

All process and resource values are simulated. The application does not change host process priorities, allocate physical RAM, or access actual devices.

## Short demonstration

1. Load the sample and point out task types, arrival ticks, and resource requests.
2. Step several ticks to show EDF preemption, user queues, and CPU timeline entries.
3. Play, then pause; show I/O blocking and its simulated completion.
4. Select a process, suspend/resume it, then terminate a different process and show memory release.
5. Review the metrics and reset the run.
