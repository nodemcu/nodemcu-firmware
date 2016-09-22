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
    `LUA_MODULE_XYZ`, and the existing `NODEMCU_MODULE()` macros
    will take care of the rest. Example Kconfig entry:
    ```
    config LUA_MODULE_XYZ
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
between the threads.

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

