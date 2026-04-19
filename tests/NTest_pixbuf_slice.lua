-- Tests for slices, i.e. pixbufs that do not have their own data and
-- instead refer to a different pixbuf.
--
-- Set `SliceSteps` before running this to configure the step with
-- which slices to be tested should be constructed.
--
-- For example: `SliceSteps = {-2} dofile"NTest_pixbuf_slice.lua"`.
--
-- (Note that this seems to exhaust resources if given more than
-- one element, perhaps because the number of test cases is already
-- fairly large without multiplying them.)
N = ...
N = (N or require "NTest")("pixbuf")
local pixbuf = require"pixbuf"
SliceSteps = SliceSteps or {2}

if unpack == nil then unpack = table.unpack end

local base_buffers = {}  -- Set in `wrapper()`
local current_slice_step = 0  -- Set in `wrapper()`

local check_byte = string.char(0xda)
local check_pattern = '()[^' .. check_byte .. ']'

-- `_NTest_pixbuf_lib` uses this to know how to create a buffer for testing.
--
-- Suppose for example: `SliceStep = -2 dofile"NTest_pixbuf_slice.lua"`.
-- Then, when a test needs a pixbuf of length 3, say, we will first construct
-- a base pixbuf of length >= 6 and then return a view into that, with step -2.
-- Since the properties tested by `_NTest_pixbuf_lib.lua` should hold for all
-- pixbufs, this is expected to be invisible to those tests.
function NewBuffer(npix, nchan)
  local step = current_slice_step
  local u_step = math.abs(step)
  local padding = u_step + 1
  local base_npix = padding + ((npix-1) * u_step + 1) + padding
  local base = pixbuf.newBuffer(base_npix, nchan)

  base:set(1, check_byte:rep(base_npix * nchan))
  local zero_pix = string.char(0):rep(nchan)
  for i = padding+1, base_npix-padding, u_step do base:set(i, zero_pix) end

  local view
  if step > 0 then
    view = base:slice(padding+1, -(padding+1), step)
  else
    view = base:slice(-(padding+1), padding+1, step)
  end
  assert(view:size() == npix)

  table.insert(base_buffers, {base, padding, step})

  return view
end

-- Makes sure that the base buffer was not affected by writes
-- to a view of it, except in locations that are visible to that view.
local function check_buffer(buffer, padding, step)
  local check_pix = check_byte:rep(buffer:channels())
  local u_step = math.abs(step)
  for i = padding+1, buffer:size() - padding, u_step do
    buffer:set(i, check_pix)
  end

  local bad = nil
  for i, _ in buffer:dump():gmatch(check_pattern) do
    if not bad then bad = {} end
    table.insert(bad, i)
  end
  if bad then print(buffer) end
  return bad
end

local function check_buffers(bb)
  local bads = nil
  for _, details in pairs(bb) do
    local bad = check_buffer(unpack(details))
    if bad then
      if not bads then bads = {} end
      table.insert(bads, bad)
    end
  end
  if bads then
    for _, bad in ipairs(bads) do print(unpack(bad)) end
  end
  ok(eq(#(bads or ''), 0), 'no overruns')
end

-- Wrap test functions to do overrun checks, and annotate test names.
local function wrapper(step, f)
  current_slice_step = step
  local bb = {}
  base_buffers = bb
  f()
  base_buffers = nil
  check_buffers(bb)
end

local n_test = N.test

N.test = function(name, f)
  for _, step in ipairs(SliceSteps) do
    n_test(string.format("%s (slice step %d)", name, step), function() wrapper(step, f) end)
  end
end

dofile"_NTest_pixbuf_lib.lua"
