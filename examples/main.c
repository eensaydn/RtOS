/**
 * main.c - Demo: 3 cooperative tasks running in round-robin
 *
 * Demonstrates the RtOS scheduler with three simple tasks:
 *   - Task A: prints a counter, yields, runs for 10 iterations
 *   - Task B: prints a counter, yields, runs for 10 iterations
 *   - Task C: prints a counter, yields, terminates after 5 iterations
 *
 * Expected output shows interleaved execution proving round-robin scheduling,
 * with Task C dropping out halfway through.
 */

#include <stdio.h>
#include "scheduler.h"

static void task_a(void)
{
    for (int i = 1; i <= 10; i++) {
        printf("  [Task A] step %d\n", i);
        scheduler_yield();
    }
}

static void task_b(void)
{
    for (int i = 1; i <= 10; i++) {
        printf("  [Task B] step %d\n", i);
        scheduler_yield();
    }
}

static void task_c(void)
{
    for (int i = 1; i <= 5; i++) {
        printf("  [Task C] step %d\n", i);
        scheduler_yield();
    }
    /* Task C returns after 5 iterations -- automatically terminated */
}

int main(void)
{
    printf("=== RtOS Cooperative Scheduler Demo ===\n\n");

    scheduler_init();

    scheduler_create_task("Task A", task_a);
    scheduler_create_task("Task B", task_b);
    scheduler_create_task("Task C", task_c);

    printf("\n");
    scheduler_run();

    printf("\n=== Demo complete ===\n");
    return 0;
}
