/**
 * config.h - Configuration constants for RtOS cooperative scheduler
 *
 * Tune these values for your application's requirements.
 */

#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

/** Maximum number of concurrent tasks the scheduler can manage. */
#define MAX_TASKS 8

/** Default stack size in bytes allocated for each task. */
#define DEFAULT_STACK_SIZE 8192

/** Maximum length of a task name (including null terminator). */
#define TASK_NAME_MAX 32

#endif /* RTOS_CONFIG_H */
