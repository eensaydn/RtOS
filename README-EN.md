# RtOS - Cooperative Task Scheduler

A micro-kernel style cooperative task scheduler written in pure C. Runs on a single core with round-robin scheduling.

## Architecture

```
include/
├── config.h           # Configuration constants (MAX_TASKS, stack size)
├── task.h             # Task struct (TCB) and task management API
└── scheduler.h        # Scheduler public API

src/
├── scheduler.c        # Core scheduler: init, run loop, yield, kill
├── task.c             # Task creation/destruction from static pool
└── context_switch.c   # Low-level context save/restore (platform-specific)

examples/
└── main.c             # Demo with 3 tasks showing round-robin behavior
```

### How It Works

1. **Static task pool** -- all tasks are pre-allocated in a fixed-size array (`MAX_TASKS` slots). No heap allocation (`malloc`) is used anywhere.

2. **Per-task execution contexts** -- each task gets its own stack and CPU state. On Windows, this is implemented via the Fibers API (`CreateFiber`/`SwitchToFiber`), which provides OS-managed per-task stacks with proper exception handling support.

3. **Cooperative yielding** -- tasks call `scheduler_yield()` to voluntarily return control to the scheduler. There is no preemption; a task that never yields will starve all others.

4. **Round-robin loop** -- `scheduler_run()` iterates over all tasks in order. For each `READY` task, it switches to that task's context. When the task yields, control returns to the scheduler, which picks the next task.

5. **Context switching** -- abstracted in `src/context_switch.c`. On Windows, uses the Fibers API. See the detailed comments in that file for why `setjmp`/`longjmp` doesn't work on Windows x86_64 (SEH unwinding prevents re-entering stack frames) and how the Fiber-based approach solves this.

### Context Switch Diagram

```
scheduler_run()                   task function
================                  ===============
context_switch_to(task) --------> [task runs]
                                   scheduler_yield()
                                     context_switch_to_scheduler()
<---- [scheduler resumes]
[pick next READY task]
context_switch_to(next) --------> [next task resumes]
                                   ...
```

### Why Fibers Instead of setjmp/longjmp?

The original design used `setjmp`/`longjmp` with direct `jmp_buf` manipulation for per-task stacks. However, on Windows x86_64, MinGW's `longjmp` performs SEH (Structured Exception Handling) unwinding via `RtlUnwindEx`. This means:

- You can `longjmp` **upward** on the call stack (to a caller's `setjmp`)
- You **cannot** jump back **into** a function you previously jumped out of

This "no re-entry" constraint makes `setjmp`/`longjmp` unusable for cooperative scheduling, where the scheduler must repeatedly enter and exit task functions. Windows Fibers are the OS-provided solution for exactly this use case.

On POSIX platforms (Linux/macOS), `setjmp`/`longjmp` or `ucontext` can be used instead. See `context_switch.c` for porting notes.

## Building

### Prerequisites

- GCC (MinGW-w64 on Windows)
- GNU Make (`mingw32-make` on Windows)

Install on Windows via Chocolatey: `choco install mingw -y`

### Compile

```bash
# Debug build (default)
mingw32-make

# Release build (optimized)
mingw32-make release

# Clean build artifacts
mingw32-make clean
```

### Run

```bash
mingw32-make run
# or directly:
./build/rtos_demo.exe
```

### Expected Output

```
=== RtOS Cooperative Scheduler Demo ===

[Scheduler] Created task 'Task A' (id=0)
[Scheduler] Created task 'Task B' (id=1)
[Scheduler] Created task 'Task C' (id=2)

[Scheduler] Starting run loop with 3 task(s).
  [Task A] step 1
  [Task B] step 1
  [Task C] step 1
  [Task A] step 2
  [Task B] step 2
  [Task C] step 2
  ...
  [Task C] step 5
  [Task A] step 6
  [Task B] step 6
[Scheduler] Task 'Task C' (id=2) terminated.
  [Task A] step 7
  [Task B] step 7
  ...
  [Task A] step 10
  [Task B] step 10
[Scheduler] Task 'Task A' (id=0) terminated.
[Scheduler] Task 'Task B' (id=1) terminated.
[Scheduler] All tasks completed. Exiting run loop.

=== Demo complete ===
```

## Configuration

Edit `include/config.h` to tune:

| Constant | Default | Description |
|----------|---------|-------------|
| `MAX_TASKS` | 8 | Maximum concurrent tasks |
| `DEFAULT_STACK_SIZE` | 8192 | Per-task stack in bytes |
| `TASK_NAME_MAX` | 32 | Max task name length |

## Limitations

- **No preemption** -- tasks must cooperatively yield. A stuck task blocks everything.
- **Windows only (currently)** -- uses Windows Fibers API. POSIX support (via `ucontext` or `setjmp`/`longjmp`) is stubbed but not implemented. See `context_switch.c` for porting notes.
- **Single core only** -- no SMP support; this is a cooperative scheduler, not an OS kernel.
