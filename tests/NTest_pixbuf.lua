-- Tests for base pixbufs, i.e. those that have their own data and are not
-- views of other pixbufs.
--
-- (Views are tested in `NTest_pixbuf_slice.lua`.)
N = ...
N = (N or require "NTest")("pixbuf")
local pixbuf = require"pixbuf"

-- `_NTest_pixbuf_lib` uses this to know how to create a buffer for testing.
-- For testing base pixbufs, `newBuffer` can be called directly.
NewBuffer = pixbuf.newBuffer

dofile"_NTest_pixbuf_lib.lua"

N.test('initialize a buffer', function()
    local buffer = pixbuf.newBuffer(9, 3)
    nok(buffer == nil)
    ok(eq(buffer:size(), 9), "check size")
    ok(buffer:base() == nil, "not a view")
    ok(eq(buffer:dump(), string.char(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)), "initialize with 0")

    fail(function() pixbuf.newBuffer(9, -1) end, "should be a positive integer")
    fail(function() pixbuf.newBuffer(0, 3) end, "should be a positive integer")
    fail(function() pixbuf.newBuffer(-1, 3) end, "should be a positive integer")
end)

--[[
pixbuf.buffer:__concat()
--]]
