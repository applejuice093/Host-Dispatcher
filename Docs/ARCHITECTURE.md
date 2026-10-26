# Architecture and Design

## Layers

```text
Qt Widgets dashboard
        |
Simulation (clock, process registry and event timeline)
  |                    |              |              |
HostDispatcher  MemoryManager   IOManager   MetricsCollector
   |       |
EDF     MLFQScheduler
   |
Process models and legal state transitions
```

The scheduling and resource code in `src/simulation.*` has no Qt dependency. `MainWindow` renders its state and forwards user actions to `Simulation`.

## Process model and validation

Each process has a unique positive ID, name, type, state, arrival tick, CPU burst and remaining CPU, memory request, and scheduling/accounting fields. A real-time process stores a positive relative deadline; its absolute deadline is `arrival + relativeDeadline`. Optional one-shot I/O configuration specifies a simulated device, CPU offset, and service duration. Input is validated before registry insertion. New processes are admitted when their arrival tick is reached; memory allocation is atomic. A process that does not fit stays New and admission is retried after resources are released.

Legal process state transitions are enforced in the simulation layer. Suspended processes are not schedulable. If I/O completes while a process is SuspendedBlocked, it becomes SuspendedReady; resuming it makes it Ready.

## Scheduling policies

- Eligible real-time tasks preempt user tasks. EDF selects the smallest absolute deadline, then earlier arrival, then smaller process ID.
- User processes use Q0 round robin with a 2 tick quantum, Q1 round robin with a 4 tick quantum, and Q2 FCFS.
- Full quantum use demotes a user task one level. I/O yield retains its current level and resets consumed quantum.
- User tasks waiting at Q1 or Q2 are boosted to Q0 after 20 Ready ticks by default. `setPriorityBoostInterval` configures this interval; zero disables boosts.
- Every enqueue first removes any prior queue entries for that process. No blocked or suspended task is placed in a ready queue.

## Resources

The default simulated memory capacity is 1024 MB. Capacity can be changed before any process is added and before the clock advances. Admission allocates memory once; termination releases it. No physical memory is allocated. Each named I/O device has one active request and a FIFO waiting queue. Service duration and completion use simulated ticks; no hardware or wall-clock sleeps are involved. Terminating a process removes its request from active and waiting device entries.

## Tick order and time boundaries

At tick `t`, `Simulation::step` performs these operations in order:

1. Admit due New processes if memory is available.
2. Complete device requests due at or before `t` and wake eligible processes.
3. Mark unfinished real-time tasks whose absolute deadline is at or before `t` as late.
4. Pick or preempt the CPU task according to EDF precedence and MLFQ priority.
5. Execute one CPU unit and append the process ID (zero means idle) for interval `[t, t+1)`.
6. Handle CPU completion, I/O blocking, or quantum expiration; release memory on termination.
7. Advance the clock to `t+1`, account for processes that waited Ready during the interval, and apply boosts for the next dispatch.
8. Update deadline misses at the new time boundary.

A task that completes at its absolute deadline is on time. Each simulated tick creates exactly one timeline entry. Arrival at tick `t` can execute during `[t,t+1)`.

## Metrics

- Waiting time adds one per interval in Ready state when not selected; Blocked and suspended time are excluded.
- Turnaround is `completion - arrival`; response is `firstStart - arrival`.
- CPU utilization is busy timeline ticks divided by elapsed ticks, as a percentage.
- Throughput is normally completed processes divided by elapsed ticks.
- Deadline miss rate is missed real-time tasks divided by configured real-time tasks; a task explicitly terminated before missing its deadline is excluded from the denominator.
- Averages and rates are zero when their denominators are zero. Explicitly terminated processes release resources but do not count as normally completed work.
- Deadline misses are recorded once per real-time process and are shown as a count.

Metrics are derived from process records and the tick timeline, not estimates. Reset clears processes, queues, resources, messages, timeline, metrics, clock, and play state.

## Known scope limits

The first complete version supports one configured I/O request per task and one CPU. Scenario persistence, exports, multi-core scheduling, and Qt Test UI automation are not included. The application simulates an operating-system dispatcher; it never controls host processes, actual RAM, or hardware.
