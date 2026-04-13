/**
 * context_switch.c - Low-level context save/restore for cooperative scheduling
 *
 * This is the trickiest part of the scheduler. We need to:
 *   1. Give each task its own private stack
 *   2. Save/restore CPU state when switching between tasks
 *   3. Handle initial task launch (first time a task runs)
 *
 * PLATFORM CONSIDERATIONS:
 * ========================
 * The classic approach is setjmp/longjmp with jmp_buf manipulation to set up
 * per-task stacks. However, on Windows x86_64, this doesn't work because
 * MinGW's longjmp() performs SEH (Structured Exception Handling) unwinding
 * via RtlUnwindEx. This means you can only longjmp "upward" on the call
 * stack -- you cannot jump back INTO a function you previously jumped out
 * of, which is exactly what cooperative scheduling requires.
 *
 * SOLUTION:
 * =========
 * - On Windows: Use the Windows Fibers API (CreateFiber / SwitchToFiber).
 *   Fibers are user-mode cooperative threads provided by the OS -- they
 *   handle stack allocation, context save/restore, and SEH state correctly.
 *   This is the officially supported mechanism for user-mode context
 *   switching on Windows.
 *
 * - On POSIX (Linux, macOS): Use setjmp/longjmp with jmp_buf manipulation.
 *   Note: glibc uses pointer mangling, so a platform-specific approach
 *   (ucontext or sigaltstack trick) may be needed. Stubbed for future.
 *
 * Despite the platform-specific implementation, the scheduler layer above
 * uses the same API (context_init, context_switch_to, etc.) regardless of
 * platform.
 */

#include <stdint.h>
#include "task.h"
#include "config.h"

#ifdef _WIN32
/* ========================================================================
 * WINDOWS IMPLEMENTATION: Fiber-based context switching
 * ========================================================================
 *
 * Windows Fibers are lightweight execution contexts that share a thread
 * but have their own stack and register state. The key API:
 *
 *   ConvertThreadToFiber(param) - Turn the calling thread into a fiber
 *   CreateFiber(stack_size, func, param) - Create a new fiber
 *   SwitchToFiber(fiber) - Save current context, restore target fiber
 *   DeleteFiber(fiber) - Free a fiber's resources
 *
 * Each fiber gets its own stack allocated by the OS, and SwitchToFiber
 * performs a full context save/restore including SEH chain, register state,
 * and stack pointer -- exactly what we need for task switching.
 */

#include <windows.h>

/**
 * The scheduler's own fiber handle.
 * This is the main thread converted to a fiber. Tasks switch back to
 * this fiber when they yield or terminate.
 */
static void *scheduler_fiber = NULL;

/**
 * Initialize the fiber subsystem.
 * Converts the main thread to a fiber so it can participate in
 * fiber switching. Must be called once before any context operations.
 *
 * @return 0 on success, -1 on failure.
 */
int context_system_init(void)
{
    scheduler_fiber = ConvertThreadToFiber(NULL);
    return scheduler_fiber ? 0 : -1;
}

/**
 * Clean up the fiber subsystem.
 * Converts the main fiber back to a regular thread.
 */
void context_system_cleanup(void)
{
    if (scheduler_fiber) {
        ConvertFiberToThread();
        scheduler_fiber = NULL;
    }
}

/**
 * Get the scheduler's fiber handle.
 *
 * @return Opaque pointer to the scheduler fiber.
 */
void *context_get_scheduler_fiber(void)
{
    return scheduler_fiber;
}

/**
 * Union for safely casting between function and object pointers.
 * ISO C forbids direct casts between these types, but the Windows
 * Fiber API requires passing a function pointer through void*.
 */
typedef union {
    void (*func)(void);
    void *ptr;
} func_ptr_cast_t;

/**
 * Fiber entry point wrapper.
 *
 * Windows fiber functions have the signature: void WINAPI func(void *param).
 * This wrapper adapts it to call our task trampoline (which takes no args).
 * The param is a pointer to the task's entry function.
 */
static void WINAPI fiber_entry(void *param)
{
    func_ptr_cast_t cast;
    cast.ptr = param;
    cast.func();
    /* If the entry function returns, we should never get here because
     * the trampoline in scheduler.c handles cleanup and switches back.
     * But as a safety net, spin forever (the scheduler will see
     * TASK_TERMINATED and never switch back to this fiber). */
    for (;;) {
        SwitchToFiber(scheduler_fiber);
    }
}

/**
 * Initialize a task's execution context (create its fiber).
 *
 * Creates a Windows Fiber with its own stack of the given size.
 * The fiber will start executing the given function when first
 * switched to.
 *
 * @param task        The task whose context to initialize.
 * @param func        Function to execute (typically the trampoline).
 * @param stack_size  Stack size in bytes for the fiber.
 * @return            0 on success, -1 on failure.
 */
int context_init(task_t *task, void (*func)(void), int stack_size)
{
    func_ptr_cast_t cast;
    cast.func = func;
    task->fiber = CreateFiber((SIZE_T)stack_size, fiber_entry, cast.ptr);
    return task->fiber ? 0 : -1;
}

/**
 * Switch execution from the current context to a task's context.
 *
 * Saves the current fiber's state and resumes the target task's fiber.
 * Returns when some other fiber switches back to us.
 *
 * @param task  The task to switch to.
 */
void context_switch_to(task_t *task)
{
    SwitchToFiber(task->fiber);
}

/**
 * Switch execution from a task back to the scheduler.
 *
 * Called by a task (from scheduler_yield or task_trampoline) to return
 * control to the scheduler's run loop.
 */
void context_switch_to_scheduler(void)
{
    SwitchToFiber(scheduler_fiber);
}

/**
 * Destroy a task's execution context (delete its fiber).
 *
 * @param task  The task whose fiber to delete.
 */
void context_destroy(task_t *task)
{
    if (task->fiber) {
        DeleteFiber(task->fiber);
        task->fiber = NULL;
    }
}

#else
/* ========================================================================
 * POSIX STUB: setjmp/longjmp based context switching
 * ========================================================================
 *
 * On Linux/macOS, setjmp/longjmp can be used for context switching.
 * However, glibc's setjmp uses pointer mangling (PTR_MANGLE), which
 * XOR-encodes RSP and RIP with a thread-local secret. This makes
 * direct jmp_buf manipulation unreliable.
 *
 * Alternatives for POSIX:
 *   - ucontext (getcontext/makecontext/swapcontext) -- deprecated but works
 *   - sigaltstack + signal handler trick (used by GNU Pth)
 *   - Custom assembly save/restore
 *
 * For now, this is stubbed out with #error.
 */

#error "POSIX context switching not yet implemented. See comments for approaches."

#endif /* _WIN32 */
