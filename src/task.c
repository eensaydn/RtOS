/**
 * task.c - Task creation, destruction, and pool management
 *
 * Tasks are allocated from a fixed-size static array (no malloc).
 * Each slot can be reused after the task is destroyed.
 */

#include <string.h>
#include "task.h"

/** Static task pool -- all task memory is pre-allocated at compile time. */
static task_t task_pool[MAX_TASKS];

/** Next task ID to assign (monotonically increasing). */
static int next_task_id = 0;

/**
 * Initialize the task pool.
 * Marks all slots as terminated (available) and resets the ID counter.
 */
void task_pool_init(void)
{
    memset(task_pool, 0, sizeof(task_pool));
    for (int i = 0; i < MAX_TASKS; i++) {
        task_pool[i].state = TASK_TERMINATED;
        task_pool[i].id = -1;
    }
    next_task_id = 0;
}

/**
 * Allocate a task from the static pool.
 *
 * Scans for the first available (TERMINATED) slot, initializes it
 * with the given name and entry function, and returns a pointer to it.
 *
 * @param name   Human-readable name (truncated to TASK_NAME_MAX - 1 chars).
 * @param entry  Function pointer to the task's main loop.
 * @return       Pointer to the new task, or NULL if the pool is full.
 */
task_t *task_create(const char *name, void (*entry)(void))
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_TERMINATED && task_pool[i].id == -1) {
            task_t *t = &task_pool[i];
            t->id = next_task_id++;
            strncpy(t->name, name, TASK_NAME_MAX - 1);
            t->name[TASK_NAME_MAX - 1] = '\0';
            t->state = TASK_READY;
            t->entry = entry;
            t->priority = 0;
            t->initialized = 0;
#ifdef _WIN32
            t->fiber = NULL;
#endif
            return t;
        }
    }
    return NULL;  /* Pool is full */
}

/**
 * Release a task back to the pool.
 *
 * Marks the task as terminated and clears its ID so the slot can be reused.
 *
 * @param task  Pointer to the task to destroy. NULL is safely ignored.
 */
void task_destroy(task_t *task)
{
    if (task) {
        task->state = TASK_TERMINATED;
        task->id = -1;
        task->initialized = 0;
    }
}

/**
 * Look up a task by its ID.
 *
 * @param id  The task ID to search for.
 * @return    Pointer to the task, or NULL if not found.
 */
task_t *task_get(int id)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_pool[i].id == id) {
            return &task_pool[i];
        }
    }
    return NULL;
}
