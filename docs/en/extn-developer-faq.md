# Extension Developer FAQ

## Firmware build options
[Building the firmware → Build Options](build.md#build-options) lists a few of the common parameters to customize your firmware *at build time*.

## How does the non-OS SDK structure execution

Details of the execution model for the **non-OS SDK** is not well documented by 
Espressif. This section summarises the project's understanding of how this execution
model works based on the Espressif-supplied examples and SDK documentation, plus
various posts on the Espressif BBS and other forums, and an examination of the
BootROM code.

The ESP8266 boot ROM contains a set of primitive tasking and dispatch functions
which are also used by the SDK. In this model, execution units are either:

-   **INTERRUPT SERVICE ROUTINES (ISRs)** which are declared and controlled
    through the `ets_isr_attach()` and other `ets_isr_*` and `ets_intr_*`
    functions. ISRs can be defined on a range of priorities, where a higher
    priority ISR is able to interrupt a lower priority one. ISRs are time
    critical and should complete in no more than 50 µSec.

    ISR code and data constants should be run out of RAM or ROM, for two reasons:
    if an ISR interrupts a flash I/O operation (which must disable the Flash 
    instruction cache) and a cache miss occurs, then the ISR will trigger a
    fatal exception; secondly, the
    execution time for Flash memory (that is located in the `irom0` load section)
    is indeterminate: whilst cache-hits can run at full memory bandwidth, any
    cache-misses require the code to be read from Flash; and even though
    H/W-based, this is at roughly 26x slower than memory bandwidth (for DIO
    flash); this will cause ISR execution to fall outside the require time
    guidelines. (Note that any time critical code within normal execution and that
    is bracketed by interrupt lock / unlock guards should also follow this 50
    µSec guideline.)<br/><br/>
    
-   **TASKS**. A task is a normal execution unit running at a non-interrupt priority.
    Tasks can be executed from Flash memory. An executing task can be interrupted
    by one or more ISRs being delivered, but it won't be preempted by another
    queued task. The Espressif guideline is that no individual task should run for
    more than 15 mSec, before returning control to the SDK.

    The ROM will queue up to 32 pending tasks at priorities 0..31 and will
    execute the highest priority queued task next (or wait on interrupt if none
    is runnable). The SDK tasking system is layered on this ROM dispatcher and
    it reserves 29 of these task priorities for its own use, including the
    implementation of the various SDK timer, WiFi and other callback mechanisms
    such as the software WDT.

    Three of these task priorities are allocated for and exposed directly at an
    application level. The application can declare a single task handler for each
    level, and associate a task queue with the level. Tasks can be posted to this
    queue. (The post will fail is the queue is full). Tasks are then delivered
    FIFO within task priority.

    How the three user task priorities USER0 .. USER2 are positioned relative to
    the SDK task priorities is undocumented, but some SDK tasks definitely run at
    a lower priority than USER0. As a result if you always have a USER task queued
    for execution, then you can starve SDK housekeeping tasks and you will start
    to get WiFi and other failures. Espressif therefore recommends that you don't
    stay computable for more than 500 mSec to avoid such timeouts.

Note that the 50µS, 15mSec and 500mSec limits are guidelines
and not hard constraints -- that is if you break them (slightly) then your code
may (usually) work, but you can get very difficult to diagnose and intermittent
failures. Also running ISRs from Flash may work until there is a collision with
SPIFFS I/O which will then a cause CPU exception.

Also note that the SDK API function `system_os_post()`, and the `task_post_*()`
macros which generate this can be safely called from an ISR.

The Lua runtime is NOT reentrant, and hence any code which calls any Lua API
must run within a task context. Any such task is what we call a _Lua-Land Task_
(or **LLT**). _ISRs must not access the Lua API or Lua resources._ LLTs can be
executed as SDK API callbacks or OS tasks. They can also, of course, call the
Lua execution system to execute Lua code (e.g. `luaL_dofile()` and related
calls).

Also since the application has no control over the relative time ordering of
tasks and SDK API callbacks, LLTs can't make any assumptions about whether a
task and any posted successors will run consecutively.

This API is designed to complement the Lua library model, so that a library can
declare one or more task handlers and that both ISPs and LLTs can then post a
message for delivery to a task handler. Each task handler has a unique message
associated with it, and may bind a single uint32 parameter. How this parameter
is interpreted is left to the task poster and task handler to coordinate.

The interface is exposed through `#include "task/task.h"` and involves two API
calls. Any task handlers are declared, typically in the module_init function by
assigning `task_get_id(some_task_callback)` to a (typically globally) accessible
handle variable, say `XXX_callback_handle`. This can then be used in an ISR or
normal LLT to execute a `task_post_YYY(XXX_callback_handle,param)` where YYY is
one of `low`, `medium`, `high`. The callback will then be executed when the SDK
delivers the task.

_Note_: `task_post_YYY` can fail with a false return if the task Q is full. 

