# Heaptrace Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2021-09-16 | [Johny Mattsson](https://github.com/jmattsson) |[Johny Mattsson](https://github.com/jmattsson) | [heaptrace.c](../../components/modules/heaptrace.c)|

The heaptrace module is a debug aid for diagnosing heap allocations and
leaks thereof. It provides an interface to the [built-in heap tracing
support in the IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html), enabling interactive heap tracing
from the Lua command line.

To use this module, "standalone" heap tracing must be enabled in Kconfig
first. This setting is found (at the time of writing) under
"Component config" > "Heap memory debugging" > "Heap tracing". The depth
of the call chain to capture can also be configured there. It can be very
beneficial to set that to its max, but it comes at significantly increased
memory requirements in turn.

A typical sequence of commands for a heap tracing session will be:
```lua
heaptrace.init(200) -- your needed buffer size will depend on your use case
collectgarbage() -- start with as clean a slate as possible
heaptrace.start(heaptrace.TRACE_LEAKS) -- start in leak tracing mode
heaptrace.dump() -- dump the current state of allocations
... -- do the thing that is leaking memory
collectgarbage() -- free things that can be freed, to avoid false positives
heaptrace.stop() -- optional, but provides higher quality output next
heaptrace.dump() -- dump the current state of allocations, hopefully showing the leaked entries and where they got allocated from
-- heaptrace.resume() -- optionally resume the tracing, if stopped previously
```

## heaptrace.init
Allocates and registers the heap tracing buffer.

#### Syntax
```lua
heaptrace.init(bufsz)
```

#### Parameters
- `bufsz` the size of the heap tracing buffer, expressed as number of heap tracing records. The actual size depends on the call chain depth configured in Kconfig.

#### Returns
`nil`


## heaptrace.start
Starts the heap tracing. All allocations and frees will be recorded in the
heap tracing buffer, subject to availability within the buffer.

#### Syntax
```lua
heaptrace.start(mode)
```

#### Parameters
- `mode` the heap tracing mode to use. One of:
  - `heaptrace.TRACE_LEAKS`
  - `heaptrace.TRACE_ALL`

#### Returns
`nil`


## heaptrace.stop
Stops the heap tracing session. A stopped session may be resumed later.

#### Syntax
```lua
heaptrace.stop()
```

#### Parameters
None.

#### Returns
`nil`


## heaptrace.resume
Resumes a previously stopped heap tracing session.

#### Syntax
```lua
heaptrace.resume()
```

#### Parameters
None.

#### Returns
`nil`


## heaptrace.dump
Dumps the heap trace buffer to the console.

#### Syntax
```lua
heaptrace.dump()
```

#### Parameters
None.

#### Returns
`nil`
