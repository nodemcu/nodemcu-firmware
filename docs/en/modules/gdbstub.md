# gdbstub Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-09-18 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [gdbstub.c](../../../app/modules/gdbstub.c)|

This module provides basic source code debugging of the firmware when used in conjunction with a version of gdb built for the lx106. If you enable this module, then fatal errors (like invalid memory reads) will trap into the gdbstub. This uses UART0 to talk to GDB. If this happens while the UART0 is connected to a terminal (or some IDE like esplorer) then you will see a string starting with `$T` and a few more characters after that. This is the signal that a trap has happened, and control should be passed to gdb.

`GDB` can then be started at connected to the NodeMCU platform. If this is connected to the host system via a serial port, then the following (or close variant) ought to work:

```
gdb bin/firmwarefile.bin
target remote /dev/ttyUSB0
```

At this point, you can just poke around and see what happened, but you cannot continue execution.

In order to do interactive debugging, add a call to `gdbstub.brk()` in your Lua code. This will trigger a break instruction and will trap into gdb as above. However, continuation is supported from a break instruction and so you can single step, set breakpoints, etc. Note that the lx106 processor as configured by Espressif only supports a single hardware breakpoint. This means that you can only put a single breakpoint in flash code. You can single step as much as you like. 

## gdbstub.brk()
Enters gdb by executing a `break 0,0` instruction.

#### Syntax
`gdbstub.brk()`

## gdbstub.gdboutput()
Controls whether system output is encapsulated in gdb remote debugging protocol. This turns out not to be as useful as you would hope - mostly because you can't send input to the NodeMCU board. Also because you really only should make this call *after* you get gdb running and connected to the NodeMCU. The example below first does the break and then switches to redirect the output. This works (but you are unable to send any more console input). 

#### Syntax
`gdbstub.gdboutput(enable)`

#### Parameters
`enable` If true, then output is wrapped in gdb remote debugging protocol. If false, then it is sent straight to the UART.

#### Example

```lua
function entergdb()
  gdbstub.brk()
  gdbstub.gdboutput(1)
  print("Active")
end

entergdb()
```

#### Notes

Once you attach gdb to the NodeMCU, then any further output from the NodeMCU will be discarded (as it does not match the gdb remote debugging protocol). This may (or may not) be a problem. If you want to run under gdb and see the output from the NodeMCU, then call `gdbstub.gdboutput(1)` and then output will be wrapped in the gdb protocol and display on the gdb console. You don't want to do this until gdb is attached as each packet requires an explicit ack in order to continue.
