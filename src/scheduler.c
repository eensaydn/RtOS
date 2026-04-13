/**
 * scheduler.c - Scheduler core: init, run loop, round-robin queue
 *
 * Implements a cooperative round-robin scheduler. Tasks run until they
 * voluntarily call scheduler_yield(), at which point the scheduler picks
 * the next READY task. When a task's entry function returns, it is
 * automatically marked as TERMINATED.
 *
 * CONTROL FLOW OVERVIEW:
 * ======================
 *
 *   scheduler_run()                task entry function
 *   ================               ====================
 *   context_switch_to(task) -----> [task code runs]
 *                                   scheduler_yield()
 *   <---- context_switch_to_sched()  [saves task state]
 *   [pick next task]
 *   context_switch_to(next) -----> [task code resumes]
 *                                   ...
 *
 * The scheduler and each task run in separate execution contexts (fibers
 * on Windows, or setjmp/longjmp contexts on POSIX). Tasks always switch
 * back to the scheduler context, and the scheduler switches out to tasks.
 */

#include <stdio.h>
#include "scheduler.h"
#include "task.h"
#include "config.h"

/* Functions defined in context_switch.c */
extern int  context_system_init(void);
extern void context_system_cleanup(void);
extern int  context_init(task_t *task, void (*func)(void), int stack_size);
extern void context_switch_to(task_t *task);
extern void context_switch_to_scheduler(void);
extern void context_destroy(task_t *task);

/** Pointer to the currently executing task (NULL when in scheduler code). */
static task_t *current_task = NULL;

/** Total number of tasks that have been created. */
static int task_count = 0;

/** Array of pointers to all created tasks, in creation order. */
static task_t *tasks[MAX_TASKS];

/**
 * Trampoline function that wraps task execution.
 *
 * This function runs in the task's execution context (its own fiber/stack).
 * It calls the task's entry function, and when that function returns
 * (either normally or because the task is done), it marks the task as
 * TERMINATED and switches back to the scheduler.
 *
 * WHY A TRAMPOLINE:
 * We can't switch directly to the user's task function because we need
 * cleanup logic (marking the task terminated and returning to the scheduler)
 * to run after the task function returns. The trampoline provides this wrapper.
 */
static void task_trampoline(void)
{
    /* current_task is set by scheduler_run() before switching here */
    current_task->entry();

    /* Task function returned -- task is done */
    current_task->state = TASK_TERMINATED;
    printf("[Scheduler] Task '%s' (id=%d) terminated.\n",
           current_task->name, current_task->id);

    /* Return control to the scheduler run loop */
    context_switch_to_scheduler();
}

/**
 * Initialize the scheduler's internal state.
 * Resets the task pool, task list, counters, and sets up the
 * platform-specific context switching subsystem.
 */
void scheduler_init(void)
{
    task_pool_init();
    task_count = 0;
    current_task = NULL;
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i] = NULL;
    }
    context_system_init();
}

/**
 * Create and register a new task with the scheduler.
 *
 * Allocates a task from the pool, sets up its execution context
 * (stack + entry point via the trampoline), and adds it to the
 * scheduler's task list.
 *
 * @param name   Human-readable task name.
 * @param entry  Task entry function. Should call scheduler_yield() to cooperate.
 * @return       Task ID (>= 0) on success, -1 if pool is full.
 */
int scheduler_create_task(const char *name, void (*entry)(void))
{
    if (task_count >= MAX_TASKS) {
        return -1;
    }

    task_t *t = task_create(name, entry);
    if (!t) {
        return -1;
    }

    /*
     * Set up the task's execution context:
     * - The trampoline function will be the first thing executed
     * - It runs in its own context with a private stack
     * - When switched to, execution starts at task_trampoline()
     */
    if (context_init(t, task_trampoline, DEFAULT_STACK_SIZE) != 0) {
        task_destroy(t);
        return -1;
    }
    t->initialized = 1;

    tasks[task_count++] = t;
    printf("[Scheduler] Created task '%s' (id=%d)\n", t->name, t->id);

    return t->id;
}

/**
 * Run the scheduler's main loop.
 *
 * Iterates in round-robin order over all tasks. For each READY task:
 *   1. Set it as the current task and mark it RUNNING
 *   2. Switch to the task's execution context
 *   3. When the task yields/terminates, execution returns here
 *   4. Move to the next task
 *
 * The loop exits when no READY tasks remain.
 */
void scheduler_run(void)
{
    printf("[Scheduler] Starting run loop with %d task(s).\n", task_count);

    for (;;) {
        int found_ready = 0;

        for (int i = 0; i < task_count; i++) {
            task_t *t = tasks[i];

            if (t->state != TASK_READY) {
                continue;
            }

            found_ready = 1;
            current_task = t;
            t->state = TASK_RUNNING;

            /*
             * Switch to the task's context. This call:
             * - Saves the scheduler's current state
             * - Restores the task's saved state and resumes it
             * - Returns here when the task calls scheduler_yield() or terminates
             */
            context_switch_to(t);

            /*
             * We're back from the task. If it was just yielding (still RUNNING),
             * mark it READY for the next round. If it terminated, leave it alone.
             */
            if (t->state == TASK_RUNNING) {
                t->state = TASK_READY;
            }

            current_task = NULL;
        }

        if (!found_ready) {
            break;  /* All tasks terminated or blocked -- nothing left to do */
        }
    }

    printf("[Scheduler] All tasks completed. Exiting run loop.\n");

    /* Clean up task contexts */
    for (int i = 0; i < task_count; i++) {
        context_destroy(tasks[i]);
    }
    context_system_cleanup();
}

/**
 * Yield the CPU from the current task back to the scheduler.
 *
 * Called by a running task to cooperatively give up execution.
 * The task's context is saved, and control returns to scheduler_run().
 * The task will be resumed in a future round-robin cycle.
 *
 * MECHANISM:
 *   1. The task calls context_switch_to_scheduler()
 *   2. This saves the task's registers/stack and restores the scheduler's
 *   3. Execution resumes in scheduler_run() after the context_switch_to() call
 *   4. Later, the scheduler calls context_switch_to(task) to resume it
 *   5. The task's context is restored and yield() returns normally
 */
void scheduler_yield(void)
{
    if (!current_task) {
        return;  /* Safety: do nothing if called outside a task context */
    }

    /*
     * Switch back to the scheduler. This saves our current state and
     * resumes the scheduler. When the scheduler later switches back to
     * us, this call returns and yield() returns to the task.
     */
    context_switch_to_scheduler();
}

/**
 * Terminate a task by its ID.
 *
 * Marks the task as TERMINATED so it won't be scheduled again.
 * Can be called from any context (another task or before scheduler_run).
 *
 * @param task_id  ID of the task to kill.
 */
void scheduler_kill(int task_id)
{
    task_t *t = task_get(task_id);
    if (t) {
        t->state = TASK_TERMINATED;
        printf("[Scheduler] Killed task '%s' (id=%d)\n", t->name, t->id);
    }
}
