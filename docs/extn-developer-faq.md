# Extension Developer FAQ

**# # # Work in Progress # # #**

### Changes in IDF compared to non-OS/RTOS SDK
This is a non-exhaustive list, obviously, but some key points are:

  - **No more `c_types.h`**. Standard C types are finally the norm.
    - `stdint.h` for all your `[u]intX_t` needs
    - `stdbool.h` for bool (note `true`/`false` vs old `TRUE`/`FALSE`)
    - `stddef.h` for `size_t`

  - **A real C library**. All the `os_`, `ets_` and `c_` prefixes for
    standard library functions are gone (except for the special case
    c_getenv, for now). stdout/stdin are wired up to the UART0 console.
    The NodeMCU vfs layer is not currently hooked up to the C library,
    but that would be a nice thing to do.

  - **Everything builds on at least C99 level**, with plenty of warnings
    enabled. Fix the code so it doesn't produce warnings - don't turn
    off the warnings! Yes, there *may* be exceptions, but they're rare.

  - **`user_config.h` is no more**. All configuration is now handled
    via Kconfig (`make menuconfig` to configure). From the developer
    perspective, simply include `sdkconfig.h` and test the
    corresponding CONFIG_YOUR_FEATURE macro. The `platform.h` header
    is guaranteed to include `sdkconfig.h`, btw.

  - **`user_modules.h` is also gone**. Module selection is now done
    via Kconfig. Rather than adding a #define to `user_modules.h`,
    add an option in `components/modules/Kconfig` of the form
    `NODEMCU_CMODULE_XYZ`, and the existing `NODEMCU_MODULE()` macros
    will take care of the rest. Example Kconfig entry:
    ```
    config NODEMCU_CMODULE_XYZ
      bool "Xyz module"
      default "y"
      help
          Includes the XYZ module. Provides features X, Y and Z.
    ```

  - **Preemptive multithreading**. The network stack and other drivers
    now run in their own threads with private stacks. This makes for
    a more robust architecture, but does mean proper synchronization
    *MUST* be employed between the threads. This includes between
    API callbacks and main Lua thread.
    Note that many API callbacks have turned into events in the IDF,
    and NodeMCU handle those in the context of the main Lua thread.

  - **Logical flash partitions**. Rather than hardcoding assumptions of
    flash area usage, there is now an actual logical partition table
    kept on the flash.

### NodeMCU task paradigm

NodeMCU uses a task based approach for scheduling work to run within
the Lua VM. This interface is defined in `task/task.h`, and comprises
three aspects:
  - Task registration (via `task_getid()`)
  - Task posting (via `task_post()` and associated macros)
  - Task processing (via `task_pump_messages()`)

This NodeMCU task API is designed to complement the Lua library model, so
that a library can declare one or more task handlers and that both ISRs
and library functions can then post a message for delivery to a task handler.

Note that NodeMCU tasks must not be confused with the FreeRTOS tasks.
A FreeRTOS task is fully preemptible thread of execution managed by the
OS, while a NodeMCU task is a non-preemptive\* a callback invoked by the
Lua FreeRTOS task. It effectively implements cooperative multitasking.
To reduce confusion, from here on FreeRTOS tasks will be referred to as
threads, and NodeMCU tasks simply as tasks.  Most NodeMCU developers
will not need to concern themselves with threads unless they're doing
low-level driver development.

The Lua runtime is NOT reentrant, and hence any code which calls any
Lua API must run within the Lua task context. _ISRs, other threads and API
callbacks must not access the Lua API or Lua resources._ This typically
means that messages need to be posted using the NodeMCU task API for
handling within the correct thread context. Depending on the scenario,
data may need to be put on a FreeRTOS queue to transfer ownership safely
between the threads, or posted directly across with the NodeMCU task API.

The application has no control over the relative time ordering of
tasks and API callbacks, and no assumptions can be made about whether a
task and any posted successors will run consecutively.

\*) Non-preemptive in the sense that other NodeMCU tasks will not preempt it.
The RTOS may still preempt the task to run ISRs or other threads.

#### Task registration
Each module which wishes to receive events and process those within the
context of the Lua thread need to register their callback. This is done
by calling `task_get_id(module_callback_fn)`. A non-zero return value
indicates successful registration, and the returned handle can be used
to post events to this task. The task registration is typically done in
the module\_init function.

#### Task posting
To signal a task, a message is posted to it using
`task_post(prio, handle, param)` or the helper macros `task_post_low()/task_post_medium()/task_post_high()`. Each message carries a single parameter value
of type `intptr_t` and may be used to carry either pointers or raw values.
Each task defines its own schema depending on its needs.

Note that the message queues can theoretically fill up, and this is reported
by a `false` return value from the `task_post()` call. In this case no
message was posted. In most cases nothing much can be done, and the best
approach may be to simply ignore the failure and drop the data. Be careful
not to accidentally leak memory in this circumstance however! A good
habit would be to do something like this:
```
  char *buf = malloc (len);
  // do something with buf...
  if (!task_post_medium(handle, buf))
    free (buf);
```

The `task_post*()` function and macros can be safely called from any ISR or
thread.

#### Task processing and priorities
The Lua runtime executes in a single thread, and at its root level runs
the task message processing loop. Task messages arrive on three queues,
one for each priority level. The queues are services in order of their
priority, so while a higher-priority queue contains messages, no
lower-priority messages will be delivered. It is thus quite possible to
jam up the Lua runtime by continually posting high-priority messages.
Don't do that.

Unless there are particular reasons not to, messages should be posted
with medium priority. This is the friendly, cooperative level. If something
is time sensitive (e.g. audio buffer running low), high priority may be
warranted. The low priority level is intended for background processing
during otherwise idle time.


### Processing system events
The IDF is quite flexible in how system events may be handled, and NodeMCU
takes advantage of this to get all system events handled by the main Lua
thread. Event listening registration is very similar to module registration,
and happens at link-time. The following snippet shows how to use this:
```
#include "nodemcu_esp_event.h"

static void on_got_ip (const system_event_t *evt)
{
  // Do stuff, maybe invoke a Lua callback
}

// Register for the event SYSTEM_EVENT_STA_GOT_IP (see esp_event.h for list).
NODEMCU_ESP_EVENT(SYSTEM_EVENT_STA_GOT_IP, on_got_ip);
```


### Memory allocation in modules
There are three main ways of allocating memory when writing modules for
NodeMCU - stack (for temporaries), heap, and Lua heap. Using the stack
is the same as for other embedded development, i.e. feel free to put
small(ish) temporary objects there, but don't expect to get away with
hundreds of bytes on the stack. Under good circumstances RTOS will detect
a stack overflow and throw an error, under less good circumstances anything
can happen.

Heap allocation is through the standard C malloc and friends. Their use
is best limited to third-party libraries which do not allow for custom
allocators. Also, if dynamic memory allocation is done in a thread other
than the main Lua thread, this is the only available option.

The best way to allocate memory in a module however, is to use the luaM
memory allocation routines. What makes this the best option is that an
allocation made this way may trigger a garbage collection. Where a regular
C malloc might have failed due to out of memory, the Lua allocation can
succeed by virtue of freeing up memory first. The downside of course is
that this is only possible to do while running in the main Lua thread.
Note that memory allocated via e.g. `luaM_malloc()` is not subject to
garbage collection, it behaves just like memory from the C `malloc()`,
and must be explicitly free'd via `luaM_free()`.

A quick example:
```
static int some_func (lua_State *L)
{
  char *buf = luaM_malloc (L, 512);
  int result = calc_using_large_buf (buf, 1, 2, 3);
  lua_pushinteger (L, result);
  luaM_free (buf);
  return 1;
}
```

#### Caution
Do note that `luaM_malloc()` raises a Lua error on allocation failure, and
will exit the calling function right then and there. This can lead to
resource leaks if care is not taken, though in most cases a failure
will result in a Lua panic and require a reboot.

On the upside, there is never a need to test the return value from
`luaM_malloc()` for NULL, as on failure the function does not return.

