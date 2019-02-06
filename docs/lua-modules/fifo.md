# FIFO Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-02-10 | [nwf](https://github.com/nwf) | [nwf](https://github.com/nwf) | [fifo.lua](../../lua_modules/fifo/fifo.lua) |

This module provides flexible, generic FIFOs built around Lua tables and
callback functions.  It is specifically engineered to work well with the
NodeMCU event-based and memory-constrained environment.

## Constructor
```lua
fifo = (require "fifo").new()
```

## fifo.dequeue()
#### Syntax
`fifo:dequeue(k)`

Fetch an element from the fifo and pass it to the function `k`, together with a
boolean indicating whether this is the last element in the fifo.  If the fifo
is empty, `k` will not be called and the fifo will enter "immediate dequeue"
mode (see below).

Assuming `k` is called, ordinarily, `k` will return `nil`, which will cause the
element given to `k` to be removed from the fifo and the queue to advance.  If,
however, `k` returns a non-`nil` value, that value will replace the element at
the head of the fifo.  This may be useful for generators, for example, which
stand in for several elements.

When `k` returns `nil`, it may also return a boolean as its second result.  If
that is `false`, processing ends and `fifo:dequeue` returns.  If that is
`true`, the fifo will be advanced again (i.e. `fifo:dequeue(k)` will be *tail
called*).  Elements for which `k` returns `nil, true` are called "phantom", as
they cause the fifo to act as though they were not there.  Phantom elements are
useful for callback-like behavior as the fifo advances: when `k` sees a phantom
element, it knows that all prior entries in the fifo have been seen, but the
phantom does not necessarily know how to generate the next element of the fifo.

#### Returns
`true` if the queue contained at least one non-phantom entry, `false` otherwise.

## fifo.queue()
#### Syntax
`fifo:queue(a,k)`

Enqueue the element `a` onto the fifo.  If `k` is not `nil` and the fifo is in
"immediate dequeue" mode (whence it starts), immediately pass the first element
of the fifo (usually, but not necessarily, `a`) to `k`, as if `fifo:dequeue(k)`
had been called, and exit "immediate dequeue" mode.

## FIFO Elements

The elements stored in the FIFO are simply the integer indices of the fifo
table itself, with `1` being the head of the fifo.  The depth of the queue for
a given `fifo` is just its table size, i.e. `#fifo`.  Direct access to the
elements is strongly discouraged.  The number of elements in the fifo is also
unlikely to be of interest; especially, decisions about the fifo's emptiness
should instead be rewritten to use the existing interface, if possible, or may
peek a bit at the immediate dequeueing state (see below).  See the discussion
of corking, below, too.

## Immediate Dequeueing

The "immediate dequeue" behavior may seem counterintuitive, but it is very
useful for the case that `fifo:dequeue`'s `k` arranges for subsequent
invocations of `fifo:dequeue`, say by scheduling the next invocation of a timer
or by sending on a socket with an `on("sent")` callback wired to
`fifo:dequeue`.

Because the fifo enters "immediate dequeue" mode only when `dequeue` has been
called and the fifo was empty at the time of the call, rather than when the
fifo *becomes* empty, `fifo:queue` will sometimes not invoke its `k` even if
the queued element `a` ends up at the front of the fifo.  This, too, is quite
useful: it ensures that `k` will not be called in contexts where it would
overlap any ongoing processing of the most-recently dequeued, fifo-emptying
element.

The immediate deququeing status of the fifo is visible as the `_go` member,
which may be read (even if said reads are politely discouraged, but on occasion
it is handy to know) but should never be written.

## Corking

The fifo has no special support for corking (that is, queueing several elements
which are guaranteed to not be dequeued until some later point, called
"uncorking").  As one often wants to cork only when the fifo is transitioning
out of immediate deququeing mode, the existing machinery is generally good
enough to provide an easy emulation thereof.  While it is typical to pass the
same `k` to both `:queue` and `:dequeue`, there is nothing necessitating this
convention.  And so one may, as in the `fifosock` module, use the `:queue`
`k` to record the transition out of immediate dequeueing mode for later,
when one wishes to uncork:

```lua
local corked = false
fifo:queue(e1, function(e) corked = true ; return e end)
  -- e1 is now in the fifo, and corked is true if the fifo has exited
  -- immediate dequeue mode.  e1 will be returned back to the fifo and
  -- so will not be deququed by the function argument.

  -- We can now queue more elements to the fifo.  These will certainly
  -- queue behind e1.
fifo:queue(e2)

  -- If we should have initiated draining the fifo above, we can do so now,
  -- instead, having built up a backlog as desired.
if corked then fifo:dequeue(k) end
```
