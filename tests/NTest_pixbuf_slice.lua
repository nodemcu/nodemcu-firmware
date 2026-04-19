-- Tests for slices, i.e. pixbufs that do not have their own data and
-- instead refer to a different pixbuf.
N = ...
N = (N or require "NTest")("pixbuf")
local pixbuf = require"pixbuf"

-- `_NTest_pixbuf_lib` uses this to know how to create a buffer for testing.
-- When a test needs a pixbuf of length 3, say, we will first construct
-- a base pixbuf and then return a view into that.
-- Since the properties tested by `_NTest_pixbuf_lib.lua` should hold for all
-- pixbufs, this is expected to be invisible to those tests.
function NewBuffer(npix, nchan)
  local base_npix = npix
  local base = pixbuf.newBuffer(base_npix, nchan)
  local view
  view = base:slice()
  assert(view:size() == npix)

  return view
end

-- Wrap test functions to do overrun checks, and annotate test names.
local n_test = N.test
N.test = function(name, f)
  local function wrapper()
    f()
  end

  return n_test(string.format("%s (slice step %d)", name, 1), wrapper)
end

dofile"_NTest_pixbuf_lib.lua"
