--
-- Set verbose to 0 for quiet output (either the first assertion failure or
-- "All tests OK"), to 1 to see the events ("SEND", "SENT", "CHECK") without
-- the actual bytes, or to 2 to see the events with the bytes.
--
local verbose = 0

local vprint = (verbose > 0) and print or function() end

--
-- Mock up enough of the nodemcu tmr structure, but pretend that nothing
-- happens between ticks.  This won't exercise the optimistic corking logic,
-- but that's probably fine.
--
tmr = {}
tmr.ALARM_SINGLE = 0
function tmr.create()
  local r = {}
  function r:alarm(_i, _t, cb) vprint("TMR") cb() end
  return r
end

--
-- Mock up enough of the nodemcu net.socket type; have it log all the sends
-- into this "outs" array so that we can later check against it.
--
local outs = {}
local fakesock = {
  cb = nil,
  on = function(this, _, cb) this.cb = cb end,
  send = function(this, s) vprint("SEND", (verbose > 1) and s) table.insert(outs, s) end,
}
local function sent() vprint("SENT") fakesock.cb() end

-- And wrap a fifosock around this fake socket
local fsend = (require "fifosock").wrap(fakesock)

-- Verify that the next unconsumed output is as indicated
local function fcheck(x)
  vprint ("CHECK", (verbose > 1) and x)
  assert (#outs > 0)
  assert (x == outs[1])
  table.remove(outs, 1)
end

-- Enqueue an empty function to prevent coalescing within the fifosock
local function nocoal() fsend(function() return nil end) end

-- Send and check, for when the string should be sent exactly as is
local function fsendc(x) fsend(x) fcheck(x) end

-- Check that there are no more outputs
local function fchecke() vprint("CHECKE") assert (#outs == 0) end

--
-- And now for the tests, which start easy and grow in complexity
--

fsendc("abracadabra none")
sent() ; fchecke()

fsendc("abracadabra three")
fsend("short")
fsend("string")
fsend("build")
sent() ; fcheck("shortstringbuild")
sent() ; fchecke()

-- Hit default FSMALLLIM while building up
fsendc("abracadabra lots small")
for i = 1, 32 do fsend("a") end
nocoal()
for i = 1, 4 do fsend("a") end
sent() ; fcheck(string.rep("a", 32))
sent() ; fcheck(string.rep("a", 4))
sent() ; fchecke()

-- Hit string length while building up
fsendc("abracadabra overlong")
for i = 1, 10 do fsend(string.rep("a",32)) end
sent() ; fcheck(string.rep("a", 320))
sent() ; fchecke()

-- Hit neither before sending a big string
fsendc("abracadabra mid long")
for i = 1, 6 do fsend(string.rep("a",32)) end
fsend(string.rep("b", 256))
nocoal()
for i = 1, 6 do fsend(string.rep("c",32)) end
sent() ; fcheck(string.rep("a", 192) .. string.rep("b", 256))
sent() ; fcheck(string.rep("c", 192))
sent() ; fchecke()

-- send a huge string, verify that it coalesces
fsendc(string.rep("a",256) .. string.rep("b", 256) .. string.rep("c", 260))
sent() ; fchecke()

-- send a huge string, verify that it coalesces save for the short bit at the end
fsend(string.rep("a",256) .. string.rep("b", 256) .. string.rep("c", 256) .. string.rep("d",256))
fsend("e")
fcheck(string.rep("a",256) .. string.rep("b", 256) .. string.rep("c", 256))
sent() ; fcheck(string.rep("d",256) .. "e")
sent() ; fchecke()

-- send enough that our 4x lookahead still leaves something in the queue
fsend(string.rep("a",512) .. string.rep("b", 512) .. string.rep("c", 512))
fcheck(string.rep("a",512) .. string.rep("b", 512))
sent() ; fcheck(string.rep("c",512))
sent() ; fchecke()

-- test a lazy generator
local ix = 0
local function gen() vprint("GEN", ix); ix = ix + 1; return ("a" .. ix), ix < 3 and gen end
fsend(gen)
fsend("b")
fcheck("a1")
sent() ; fcheck("a2")
sent() ; fcheck("a3")
sent() ; fcheck("b")
sent() ; fchecke()

-- test a completion-like callback that does send text
local ix = 0
local function gen() vprint("GEN"); ix = 1; return "efgh", nil end
fsend("abcd"); fsend(gen); fsend("ijkl")
assert (ix == 0)
         fcheck("abcd"); assert (ix == 0)
sent() ; fcheck("efgh"); assert (ix == 1); ix = 0
sent() ; fcheck("ijkl"); assert (ix == 0)
sent() ; fchecke()

-- and one that doesn't
local ix = 0
local function gen() vprint("GEN"); ix = 1; return nil, nil end
fsend("abcd"); fsend(gen); fsend("ijkl")
assert (ix == 0)
         fcheck("abcd"); assert (ix == 0)
sent() ; fcheck("ijkl"); assert (ix == 1); ix = 0
sent() ; fchecke()     ; assert (ix == 0)

print("All tests OK")
