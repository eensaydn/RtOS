/**
 * scheduler.h - Scheduler API declarations
 *
 * Provides a cooperative round-robin task scheduler. Tasks voluntarily
 * yield control back to the scheduler, which then picks the next READY
 * task in round-robin order.
 */

#ifndef RTOS_SCHEDULER_H
#define RTOS_SCHEDULER_H

/**
 * Initialize the scheduler's internal state.
 * Must be called once before creating tasks or running the scheduler.
 */
void scheduler_init(void);

/**
 * Create and register a new task with the scheduler.
 *
 * @param name   Human-readable name for the task.
 * @param entry  Function pointer to the task's entry point.
 *               The function should call scheduler_yield() to cooperate.
 *               When the function returns, the task is automatically terminated.
 * @return       Task ID on success, or -1 if the task pool is full.
 */
int scheduler_create_task(const char *name, void (*entry)(void));

/**
 * Start the scheduler's main run loop.
 *
 * Iterates round-robin over all READY tasks, switching context to each.
 * Returns when no READY tasks remain (all terminated or none created).
 */
void scheduler_run(void);

/**
 * Yield the CPU from the currently running task back to the scheduler.
 *
 * Called BY a running task to voluntarily give up execution. The scheduler
 * will resume this task in a future round-robin cycle.
 */
void scheduler_yield(void);

/**
 * Terminate a task by its ID.
 *
 * The task is marked as TERMINATED and will no longer be scheduled.
 *
 * @param task_id  The ID of the task to kill.
 */
void scheduler_kill(int task_id);

#endif /* RTOS_SCHEDULER_H */
