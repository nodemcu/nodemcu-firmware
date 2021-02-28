# gdbstub Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-09-18 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [gdbstub.c](../../app/modules/gdbstub.c)|

This module provides basic source code debugging of the firmware when used in conjunction with a version of gdb built for the lx106. If you enable this module, then fatal errors (like invalid memory reads) will trap into the gdbstub. This uses UART0 to talk to GDB. If this happens while the UART0 is connected to a terminal (or some IDE like ESPlorer) then you will see a string starting with `$T` and a few more characters after that. This is the signal that a trap has happened, and control should be passed to gdb.

`GDB` can then be started at connected to the NodeMCU platform. If this is connected to the host system via a serial port, then the following (or close variant) ought to work:

```
elf-gdb bin/firmwarefile.bin
target remote /dev/ttyUSB0
```

where `elf-gdb` is a symlink or alias pointing to the `gdb` image in your Xtensa toolchain; you cannot use the default native gdb build.

In order to do interactive debugging, add a call to `gdbstub.brk()` or `gdbstub.pbrk()` in your Lua code. This will trigger a break instruction and will trap into gdb as above.  Limited continuation is supported from a break instruction and so you can single step, set breakpoints, etc.

Note that the lx106 processor as configured by Espressif only supports a single hardware breakpoint. This means that you can only put a single breakpoint in flash code. You can single step as much as you like.

## gdbstub.open()
Runs gdbstub initialization routine. Note that subsequent calls are ignored and the break functions will do this automatically if not already done so this is options

#### Syntax
`gdbstub.open()`

## gdbstub.brk()
Enters gdb by executing a `break 0,0` instruction, and if necessary first does initialisation.

#### Syntax
`gdbstub.brk()`

## gdbstub.pbrk()
Enters gdb by executing a `break 0,0` instruction, and if necessary first does initialisation; It also set the `gdboutput` mode to 1 allowing the debug client to capture and echo UART output through the debug session.

#### Syntax
`gdbstub.pbrk()`

## gdbstub.gdboutput()
Controls whether system output is encapsulated in gdb remote debugging protocol. This turns out not to be as useful as you would hope - mostly because you can't send input to the NodeMCU board. Also because you really only should make this call *after* you get gdb running and connected to the NodeMCU. The example below first does the break and then switches to redirect the output. This works (but you are unable to send any more console input).

#### Syntax
`gdbstub.gdboutput(enable)`

#### Parameters
`enable` If true, then output is wrapped in gdb remote debugging protocol. If false, then it is sent straight to the UART.

#### Example

```Lua
-- Enter the debugger if your code throws an error 
xpcall(someTest, function(err) gdbstub.pbrk() end)
```

```Lua
someprolog(); gdbstub.pbrk(); mylib.method(args)
```

#### Notes

-  This debug functionality is aimed at assisting C library developers, who are already familiar with use of `gdb` and with some knowledge of the internal Lua APIs. Lua developers (at least with Lua 5.3 builds) are better off using the standard Lua `debug` library.

-  To get the best out of remote gdb, it helps to have reduced the error that you are investigating to a specific failing test case. This second example works because you can type in this line interactively and the Lua runtime will compile this then execute the compiled code, running the debug stub. `hb mylib_method` followed by `c` will allow the runtime to continue to the point where you enter your method under test.

-  See the `.gdbinit` and `.gdbinitlua` examples of how to customise the environment.

-  Once you attach gdb to the NodeMCU, then you can only continue to work within the current SDK task.  The session does not support continuation through the SDK to other tasks.  This means that you cannot use asynchronous services such as `net`. For this reason, the stub is really only useful for working through the forensics of why a specific bug is occurring.

-  If you compile your build with `DEVELOPMENT_TOOLS` and `DEVELOPMENT_USE_GDB` enabled in your `app/include/user_config.h`, then any `lua_assert()` API will call the `lua_debugbreak()` wrapper which also call the stub. 

-  If `gdboutput()` has not been enabled then any further output from the NodeMCU will be discarded (as it does not match the gdb remote debugging protocol). This may (or may not) be a problem. If you want to run under gdb and see the output from the NodeMCU, then call `gdbstub.gdboutput(1)` or use `gdbstub.pbrk()`.

The main functional limitation of the environment is that the ESP8266 only supports a single hardware breakpoint at any time (the gdb `hb` and `wa` instruction) and you need to use hardware breakpoints for debugging firmware based code.  This means that you cannot break on multiple code paths.  On method of mitigating this is to make liberal use of `lua_assert()` statements in your code; these will enter into a debug session on failure.  (They are optimised out on normal production builds.)
