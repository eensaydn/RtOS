/**
 * task.h - Task struct and task management API
 *
 * Defines the task control block (TCB) and low-level task operations.
 * Tasks are allocated from a static pool of MAX_TASKS entries.
 */

#ifndef RTOS_TASK_H
#define RTOS_TASK_H

#include "config.h"

/** Task lifecycle states. */
typedef enum {
    TASK_READY,       /**< Task is ready to be scheduled */
    TASK_RUNNING,     /**< Task is currently executing */
    TASK_BLOCKED,     /**< Task is waiting on a resource (future use) */
    TASK_TERMINATED   /**< Task has finished execution */
} task_state_t;

/**
 * Task Control Block (TCB).
 *
 * Each task has its own execution context (fiber on Windows, or
 * setjmp/longjmp context on POSIX) and metadata for scheduling.
 */
typedef struct {
    int             id;                         /**< Unique task identifier */
    char            name[TASK_NAME_MAX];        /**< Human-readable task name */
    task_state_t    state;                      /**< Current lifecycle state */
    void            (*entry)(void);             /**< Task entry point function */
    int             priority;                   /**< Priority level (reserved for future use) */
    int             initialized;                /**< 1 if context has been set up for first run */

#ifdef _WIN32
    /** Windows Fiber handle for this task's execution context. */
    void           *fiber;
#else
    /** Per-task stack memory (used on POSIX with setjmp/longjmp). */
    char            stack[DEFAULT_STACK_SIZE];
    /** Saved CPU context -- registers, stack pointer, program counter. */
    /* jmp_buf would go here for POSIX implementation */
#endif
} task_t;

/**
 * Initialize the task pool.
 * Must be called before any other task_* functions.
 */
void task_pool_init(void);

/**
 * Allocate a task from the static pool.
 *
 * @param name   Human-readable name for the task.
 * @param entry  Function pointer to the task's entry point.
 * @return       Pointer to the allocated task, or NULL if the pool is full.
 */
task_t *task_create(const char *name, void (*entry)(void));

/**
 * Release a task back to the pool by marking it as terminated.
 *
 * @param task  Pointer to the task to destroy.
 */
void task_destroy(task_t *task);

/**
 * Get a task by its ID.
 *
 * @param id  The task ID to look up.
 * @return    Pointer to the task, or NULL if the ID is invalid.
 */
task_t *task_get(int id);

#endif /* RTOS_TASK_H */
