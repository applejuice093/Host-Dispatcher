# User Flow and Screen Flow

## Main user journey

``` text
Launch
  |
  v
Dashboard
  |
  +--> Load Sample Scenario -------------------+
  |                                            |
  +--> Process Manager                         |
  |      +--> Add Process                      |
  |      +--> Validate Inputs                  |
  |      +--> Admit / Show Error               |
  |      +--> Select Process                   |
  |              +--> Suspend                  |
  |              +--> Resume                   |
  |              +--> Terminate                |
  |                                            |
  +--> Scheduler View <------------------------+
  |      +--> Start / Pause / Step / Reset
  |      +--> Watch EDF and MLFQ decisions
  |      +--> View CPU Gantt timeline
  |
  +--> Resource Manager
  |      +--> Memory used/free
  |      +--> I/O active requests and queues
  |
  +--> Performance Report
         +--> Metrics and deadline outcomes
         +--> Export report (optional)
```

## Add-process flow

1.  User opens Add Process.
2.  User selects Real-Time or User.
3.  Form displays fields applicable to that type.
4.  User submits the form.
5.  Domain validation checks IDs, times, CPU burst, memory, and
    deadline.
6.  Admission checks memory and arrival eligibility.
7.  On success, the process appears in the process table and scheduler
    state.
8.  On failure, show a clear reason; do not leave partial allocations or
    queue entries.

## Tick flow

``` text
Step / timer tick
    |
    v
Advance simulated clock
    |
    v
Admit due tasks and complete due I/O
    |
    v
Update deadline and priority-boost events
    |
    v
Choose CPU task (real-time EDF first, otherwise MLFQ)
    |
    v
Execute one CPU unit or record CPU idle
    |
    v
Handle completion / I/O request / quantum expiry / preemption
    |
    v
Update memory, queues, metrics, and timeline
    |
    v
Refresh UI
```

The exact event order must be documented and tested consistently,
especially when an arrival, I/O completion, deadline, and quantum expiry
occur at the same tick.

## State transition flow

``` text
New --admit--> Ready --dispatch--> Running --complete--> Terminated
                                ^      |
                                |      +--I/O request--> Blocked
                                |                          |
                                +-------I/O complete-------+
                                |
Running --preempt/quantum expiry-+

Ready --suspend--> SuspendedReady --resume--> Ready
Blocked --suspend--> SuspendedBlocked --resume--> Blocked or Ready
Running --suspend--> SuspendedReady
Any eligible non-terminated state --terminate--> Terminated
```

## Error and recovery flow

-   Invalid form: retain entered values, identify invalid field, do not
    create a process.
-   Insufficient memory: explain requested/free memory and allow retry
    after resources are released.
-   Invalid state action: disable the action or show a concise
    explanation.
-   Reset: ask for confirmation if a simulation is active; clear queues,
    process registry, resource allocations, event log, metrics, and
    tick.
