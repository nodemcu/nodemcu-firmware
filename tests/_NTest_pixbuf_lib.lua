-- Everything tested in this library should be true both for any base pixbuf
-- and for any slice/view of a pixbuf.
--
-- `NTest_pixbuf.lua` runs these tests against base pixbufs, while
-- `NTest_pixbuf_slice.lua` runs them against slices of pixbufs.
-- Each of them sets a global function `NewBuffer(npix, nchan)` which these
-- tests use to create buffers for testing.

local pixbuf = require"pixbuf"

local function initBuffer(buf, ...)
  for i,v in ipairs({...}) do
    buf:set(i, v, v*2, v*3, v*4)
  end
  return buf
end

N.test('have correct size', function()
    local buffer = NewBuffer(9, 3)
    ok(eq(buffer:size(), 9), "check size")
    buffer = NewBuffer(9, 4)
    ok(eq(buffer:size(), 9), "check size")
end)

N.test('fill a buffer with one color', function()
    local buffer = NewBuffer(3, 3)
    buffer:fill(1,222,55)
    ok(eq(buffer:dump(), string.char(1,222,55,1,222,55,1,222,55)), "RGB")
    buffer = NewBuffer(3, 4)
    buffer:fill(1,222,55,77)
    ok(eq(buffer:dump(), string.char(1,222,55,77,1,222,55,77,1,222,55,77)), "RGBW")
end)

N.test('replace correctly', function()
    local buffer = NewBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255))
    ok(eq(buffer:dump(), string.char(3,255,165,33,0,244,12,87,255,0,0,0,0,0,0)), "RGBW")

    buffer = NewBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255), 2)
    ok(eq(buffer:dump(), string.char(0,0,0,3,255,165,33,0,244,12,87,255,0,0,0)), "RGBW")

    buffer = NewBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255), -5)
    ok(eq(buffer:dump(), string.char(3,255,165,33,0,244,12,87,255,0,0,0,0,0,0)), "RGBW")

    fail(function() buffer:replace(string.char(3,255,165,33,0,244,12,87,255), 4) end,
         "does not fit into destination")
end)

N.test('replace correctly issue #2921', function()
    local buffer = NewBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255), -7)
    ok(eq(buffer:dump(), string.char(3,255,165,33,0,244,12,87,255,0,0,0,0,0,0)), "RGBW")
end)

N.test('replace correctly issue #3702', function()
    local buffer1 = NewBuffer(4, 4)
    buffer1:set(1, "AAAABBBBCCCCDDDD")
    fail(function() buffer1:replace("XXXX", 5) end,
         "does not fit into destination")
end)

N.test('get/set correctly', function()
    local buffer = NewBuffer(3, 4)
    buffer:fill(1,222,55,13)
    ok(eq({buffer:get(2)},{1,222,55,13}), "get filled value")
    buffer:set(2, 4,53,99,0)
    ok(eq({buffer:get(1)},{1,222,55,13}), "get filled value again")
    ok(eq({buffer:get(2)},{4,53,99,0}), "get set value")
    ok(eq(buffer:dump(), string.char(1,222,55,13,4,53,99,0,1,222,55,13)), "RGBW")

    fail(function() buffer:get(0) end,         "index out of range", "get i too small")
    fail(function() buffer:get(4) end,         "index out of range", "get i too large")
    fail(function() buffer:set(0,1,2,3,4) end, "index out of range", "set i too small")
    fail(function() buffer:set(4,1,2,3,4) end, "index out of range", "set i too large")
    fail(function() buffer:set(2,1,2,3) end, "number expected, got no value", "too few args")
    fail(function() buffer:set(2,1,2,3,4,5) end, "extra values given", "too many args")
end)

N.test('get/set multiple with string', function()
    -- verify that :set does indeed return its input
    local buffer = NewBuffer(4, 3):set(1,"ABCDEF")
    buffer:set(3,"LMNOPQ")
    ok(eq(buffer:dump(), "ABCDEFLMNOPQ"))

    fail(function() buffer:set(4,"AAAAAA") end, "string size will exceed strip length")
    fail(function() buffer:set(2,"AAAAA") end, "string does not contain whole LEDs")
end)

N.test('fade correctly', function()
    local buffer = NewBuffer(1, 3)
    buffer:fill(1,222,55)
    buffer:fade(2)
    ok(buffer:dump() == string.char(0,111,27), "RGB")
    buffer:fill(1,222,55)
    buffer:fade(3, pixbuf.FADE_OUT)
    ok(buffer:dump() == string.char(0,math.floor(222/3),math.floor(55/3)), "RGB")
    buffer:fill(1,222,55)
    buffer:fade(3, pixbuf.FADE_IN)
    ok(buffer:dump() == string.char(3,255,165), "RGB")
    buffer = NewBuffer(1, 4)
    buffer:fill(1,222,55, 77)
    buffer:fade(2, pixbuf.FADE_OUT)
    ok(eq(buffer:dump(), string.char(0,111,27,38)), "RGBW")
end)

N.test('mix correctly issue #1736', function()
    local buffer1 = NewBuffer(1, 3)
    local buffer2 = NewBuffer(1, 3)
    buffer1:fill(10,22,54)
    buffer2:fill(10,27,55)
    buffer1:mix(256/8*7,buffer1,256/8,buffer2)
    ok(eq({buffer1:get(1)}, {10,23,54}))
end)

N.test('mix saturation correctly ', function()
    local buffer1 = NewBuffer(1, 3)
    local buffer2 = NewBuffer(1, 3)

    buffer1:fill(10,22,54)
    buffer2:fill(10,27,55)
    buffer1:mix(256/2,buffer1,-256,buffer2)
    ok(eq({buffer1:get(1)}, {0,0,0}))

    buffer1:fill(10,22,54)
    buffer2:fill(10,27,55)
    buffer1:mix(25600,buffer1,256/8,buffer2)
    ok(eq({buffer1:get(1)}, {255,255,255}))

    buffer1:fill(10,22,54)
    buffer2:fill(10,27,55)
    buffer1:mix(-257,buffer1,255,buffer2)
    ok(eq({buffer1:get(1)}, {0,5,1}))
end)

N.test('power', function()
    local buffer = NewBuffer(2, 4)
    buffer:fill(10,22,54,234)
    ok(eq(buffer:power(), 2*(10+22+54+234)))
end)

N.test('shift LOGICAL', function()
    local buffer1 = NewBuffer(4, 4)
    local buffer2 = NewBuffer(4, 4)

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,0,7,8)
    ok(buffer1 ~= buffer2, "disequality pre shift")
    buffer1:shift(2)
    ok(eq(buffer1, buffer2), "shift right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12,0,0)
    buffer1:shift(-2)
    ok(eq(buffer1, buffer2), "shift left")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,0,8,12)
    buffer1:shift(1, nil, 2,3)
    ok(eq(buffer1, buffer2), "shift middle right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,9,0,12)
    buffer1:shift(-1, nil, 2,3)
    ok(eq(buffer1, buffer2), "shift middle left")

    -- bounds checks, handle gracefully as string:sub does
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,8,9,12,0)
    buffer1:shift(-1, pixbuf.SHIFT_LOGICAL, 0,5)
    ok(eq(buffer1, buffer2), "shift left out of bound")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,7,8,9)
    buffer1:shift(1, pixbuf.SHIFT_LOGICAL, 0,5)
    ok(eq(buffer1, buffer2), "shift right out of bound")

end)

N.test('shift LOGICAL issue #2946', function()
    local buffer1 = NewBuffer(4, 4)
    local buffer2 = NewBuffer(4, 4)

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,0,0,0)
    buffer1:shift(4)
    ok(eq(buffer1, buffer2), "shift all right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,0,0,0)
    buffer1:shift(-4)
    ok(eq(buffer1, buffer2), "shift all left")

    fail(function() buffer1:shift(10) end, "shifting more elements than buffer size")
    fail(function() buffer1:shift(-6) end, "shifting more elements than buffer size")
end)

N.test('shift LOGICAL issue #3702', function()
    local buffer1 = NewBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    fail(function() buffer1:shift(1, pixbuf.SHIFT_LOGICAL, 5, 5) end,
         "end position must be >= start")
end)

N.test('shift CIRCULAR', function()
    local buffer1 = NewBuffer(4, 4)
    local buffer2 = pixbuf.newBuffer(4, 4)

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12,7,8)
    buffer1:shift(2, pixbuf.SHIFT_CIRCULAR)
    ok(eq(buffer1, buffer2), "shift right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12,7,8)
    buffer1:shift(-2, pixbuf.SHIFT_CIRCULAR)
    ok(eq(buffer1, buffer2), "shift left")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,9,8,12)
    buffer1:shift(1, pixbuf.SHIFT_CIRCULAR, 2,3)
    ok(eq(buffer1, buffer2), "shift middle right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,9,8,12)
    buffer1:shift(-1, pixbuf.SHIFT_CIRCULAR, 2,3)
    ok(eq(buffer1, buffer2), "shift middle left")

    -- bounds checks, handle gracefully as string:sub does
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,8,9,12,7)
    buffer1:shift(-1, pixbuf.SHIFT_CIRCULAR, 0,5)
    ok(eq(buffer1, buffer2), "shift left out of bound")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,12,7,8,9)
    buffer1:shift(1, pixbuf.SHIFT_CIRCULAR, 0,5)
    ok(eq(buffer1, buffer2), "shift right out of bound")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,12,7,8,9)
    buffer1:shift(1, pixbuf.SHIFT_CIRCULAR, -12,12)
    ok(eq(buffer1, buffer2), "shift right way out of bound")

end)

N.test('slice reference', function()
  local base = NewBuffer(4, 4)
  local view = base:slice(2, 3)

  ok(rawequal(base, view:base()), "base() returns base pixbuf")

  -- Use a weak table to detect when `base` is garbage collected
  local refs = {}
  setmetatable(refs, {__mode='kv'})
  refs[base] = base

  local function empty(t)
    for k, _ in pairs(t) do return false end
    return true
  end

  base = nil
  -- Now the only strong reference to `base` is the one that `view` holds

  collectgarbage()
  ok(not empty(refs), "keeps base pixbuf alive")

  view = nil
  collectgarbage()  -- clean up `view` and run its finalizer
  collectgarbage()  -- clean up `base`
  -- No strong references to `base` remain

  ok(empty(refs), "releases base pixbuf when garbage-collected")
end)

N.test('slice whole view', function()
  local base = NewBuffer(4, 4)
  initBuffer(base,7,8,9,12)

  local view = base:slice()
  ok(eq(base:channels(), view:channels()), "preserves channel count")
  ok(eq(base:size(), view:size()), "spans entire base")
  ok(eq(base, view), "compares equal to base")

  local expected = pixbuf.newBuffer(4, 4)
  initBuffer(expected,7,8,9,12)
  ok(eq(expected, base), "leaves base unchanged")

  initBuffer(expected,1,9,8,0)
  base:replace(expected)
  ok(eq(expected, view), "reflects changes made to base")

  initBuffer(expected,2,7,4,4)
  view:replace(expected)
  ok(eq(expected, base), "writes through to base")
end)

N.test('slice partial view', function()
  local base = NewBuffer(4, 4)
  initBuffer(base,7,8,9,12)

  local half_expected = pixbuf.newBuffer(2, 4)
  local expected = pixbuf.newBuffer(4, 4)

  local half = base:slice(1,2)
  initBuffer(half_expected,7,8)
  ok(eq(half_expected, half), "compares equal to part of base")

  half:fill(0,0,0,0)
  initBuffer(expected,0,0,9,12)
  initBuffer(half_expected,0,0)
  ok(eq(expected, base), "writes through to base")
  ok(eq(half_expected, half), "reflects changes made to self")

  initBuffer(base,1,5,3,7)
  initBuffer(half_expected,1,5)
  ok(eq(half_expected, half), "reflects changes made to base")
end)

N.test('slice degenerate view', function()
  local base = NewBuffer(4, 4)
  initBuffer(base,7,8,9,12)

  for _, bounds in ipairs({
    {2, 1},  -- empty
    {5, 7}, -- too positive
    {7, 5}, -- too positive and backwards
    {-8, -6}, -- too negative
    {-6, -8}, -- too negative and backwards
    {0, 1},
    {1, 0},
    {-1, 0},
    {0, -1},
  }) do
    start, stop = unpack(bounds)
    ok(base:sub(start, stop) == base:slice(start, stop),
       ('matches sub(%d, %d)'):format(start, stop))
  end

  ok(base:slice(3, 2):base() == nil, "has no base if empty")
end)

N.test('slice with unit step', function()
  local base = NewBuffer(6, 4)
  initBuffer(base,1,2,3,4,5,6)

  local view = base:slice(2, 5, 1)
  ok(eq(base:sub(2, 5), view), 'unit step')
end)

N.test('slice with step', function()
  local base = NewBuffer(6, 4)
  initBuffer(base,1,2,3,4,5,6)

  local view = base:slice(2, 5, 2)
  ok(eq(view:stride(), 2 * base:stride()))
  ok(eq(view:size(), 2))

  local expected = pixbuf.newBuffer(2, 4)
  initBuffer(expected,2,4)

  ok(eq(expected, view), 'positive step')
end)

N.test('slice with unit negative step', function()
  local base = NewBuffer(6, 4)
  initBuffer(base,1,2,3,4,5,6)

  local view = base:slice(5, 2, -1)
  ok(eq(view:stride(), -1 * base:stride()))
  ok(eq(view:size(), 4))

  local expected = pixbuf.newBuffer(4, 4)
  initBuffer(expected,5,4,3,2)

  ok(eq(expected, view), 'unit negative step')
end)

N.test('slice with negative step', function()
  local base = NewBuffer(6, 4)
  initBuffer(base,1,2,3,4,5,6)

  local view = base:slice(5, 2, -2)
  ok(eq(view:stride(), -2 * base:stride()))
  ok(eq(view:size(), 2))

  local expected = pixbuf.newBuffer(2, 4)
  initBuffer(expected,5,3)

  ok(eq(expected, view), 'negative step')
end)

N.test('slice with longer negative step', function()
  local base = NewBuffer(6, 4)
  initBuffer(base,1,2,3,4,5,6)

  local view = base:slice(5, 1, -2)
  ok(eq(view:stride(), -2 * base:stride()))
  ok(eq(view:size(), 3))

  local expected = pixbuf.newBuffer(3, 4)
  initBuffer(expected,5,3,1)

  ok(eq(expected, view), 'longer negative step')
end)

N.test('slice of slice', function()
  local base = NewBuffer(6, 4)
  initBuffer(base,1,2,3,4,5,6)

  base:slice(2, 5):slice(2, -2):fill(0,0,0,0)

  local expected = pixbuf.newBuffer(6, 4)
  initBuffer(expected,1,2,0,0,5,6)

  ok(eq(expected, base), "mutates original base object")
end)

N.test('stride', function()
    local base = NewBuffer(4, 4)

    local view = base:slice()
    ok(eq(view:stride(), base:stride()), "of non-stepped view matches that of base")
end)

N.test('sub', function()
    local buffer1 = NewBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    buffer1 = buffer1:sub(4,3)
    ok(eq(buffer1:size(), 0), "sub empty")

    local buffer2 = pixbuf.newBuffer(2, 4)
    buffer1 = NewBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12)
    buffer1 = buffer1:sub(3,4)
    ok(buffer1 == buffer2, "sub")

    buffer1 = NewBuffer(4, 4)
    buffer2 = pixbuf.newBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,8,9,12)
    buffer1 = buffer1:sub(-12,33)
    ok(buffer1 == buffer2, "out of bounds")
end)

N.test('map', function()
    local buffer1 = NewBuffer(4, 4)
    buffer1:fill(65,66,67,68)

    buffer1:map(function(a,b,c,d) return b,a,c,d end)
    ok(eq("BACDBACDBACDBACD", buffer1:dump()), "swizzle")

    local buffer2 = NewBuffer(4, 1)
    buffer2:map(function(b,a,c,d) return c end, buffer1) -- luacheck: ignore
    ok(eq("CCCC", buffer2:dump()), "projection")

    local buffer3 = NewBuffer(4, 3)
    buffer3:map(function(b,a,c,d) return a,b,d end, buffer1) -- luacheck: ignore
    ok(eq("ABDABDABDABD", buffer3:dump()), "projection 2")

    buffer1:fill(70,71,72,73)
    buffer1:map(function(c,a,b,d) return a,b,c,d end, buffer2, nil, nil, buffer3)
    ok(eq("ABCDABCDABCDABCD", buffer1:dump()), "zip")

    buffer1 = NewBuffer(2, 4)
    buffer1:fill(70,71,72,73)
    buffer2:set(1,"ABCD")
    buffer3:set(1,"EFGHIJKLM")
    buffer1:map(function(c,a,b,d) return a,b,c,d end, buffer2, 1, 2, buffer3, 2)
    ok(eq("HIAJKLBM", buffer1:dump()), "partial zip")
end)

N.test('map issue #3702', function()
    local buffer1 = NewBuffer(4, 4)
    local buffer2 = NewBuffer(1, 4)
    local f = function(a, b, c, d) return a+10, b+10, c+10, d+10 end
    buffer1:fill(65,66,67,68)
    buffer2:map(f, buffer1, -1, -1) -- just the last pixel
    ok(eq("KLMN", buffer2:dump()), "map last pixel only")
end)

N.test('sub boundaries behave like string.sub', function()
    local buffer1 = NewBuffer(4, 4)
    local function quadruple(s)
        local q = ""
        for i = 1, #s do
            local c = s:byte(i)
            q = ("%s%c%c%c%c"):format(q, c, c, c, c)
        end
        return q
    end

    local str1 = "ABCD"
    buffer1:set(1, quadruple(str1))

    for _, range in ipairs({
        {2, 3},  -- all in range
        {2, nil},  -- implicit end
        {2, -1},  -- explicit end
        {-2, nil}, -- suffix of length 2
        {-4, nil}, -- suffix of length 4: entire sequence
        {-5, nil}, -- still entire sequence
        {-100, nil}, -- still entire sequence
        {-5, 1}, -- overlap just first element
        {-5, -3}, -- overlap first two elements
        {0, 2}, -- overlap first two elements
        {-5, 0}, -- empty
        {3, 2}, -- empty
        {3, -2}, -- just third element
        {4, 5}, -- just last element
        {4, 6}, -- still just last element
        {4, -1}, -- also just last element
        {5, 6}, -- empty
        {5, -1}, -- empty
        {6, -5}, -- empty
        {-5, 10}, -- entire sequence
    }) do
        local first, last = range[1], range[2]
        local range_name = ("(%s to %s)"):format(first or 'nil', last or 'nil')

        local sub_buf = buffer1:sub(first, last)
        local sub_str = str1:sub(first, last)

        ok(eq(sub_buf:dump(), quadruple(sub_str)), range_name)
    end
end)

N.test('channel slicing', function()
    local buffer = NewBuffer(4, 4)
    buffer:set(1, "abcdefghijklmnop");
    buffer:channel(1):fill(('X'):byte())
    ok(eq(buffer:dump(), "XbcdXfghXjklXnop"), "set channel 1")
    buffer:channel(2):fill(('Y'):byte())
    ok(eq(buffer:dump(), "XYcdXYghXYklXYop"), "set channel 2")
    buffer:channel(3):fill(('Z'):byte())
    ok(eq(buffer:dump(), "XYZdXYZhXYZlXYZp"), "set channel 3")
    buffer:channel(4):fill(('W'):byte())
    ok(eq(buffer:dump(), "XYZWXYZWXYZWXYZW"), "set channel 4")

    buffer:set(1, "abcdefghijklmnop");
    buffer:channel('g'):fill(('X'):byte())
    ok(eq(buffer:dump(), "XbcdXfghXjklXnop"), "set channel g")
    buffer:channel('r'):fill(('Y'):byte())
    ok(eq(buffer:dump(), "XYcdXYghXYklXYop"), "set channel r")
    buffer:channel('b'):fill(('Z'):byte())
    ok(eq(buffer:dump(), "XYZdXYZhXYZlXYZp"), "set channel b")
    buffer:channel('w'):fill(('W'):byte())
    ok(eq(buffer:dump(), "XYZWXYZWXYZWXYZW"), "set channel w")

    fail(function() buffer:channel(0) end, "channel number out of bounds", "too small")
    fail(function() buffer:channel(5) end, "channel number out of bounds", "too large")
    fail(function() buffer:channel('x') end, "invalid channel name", "wrong name")
    fail(function() buffer:channel('') end, "invalid channel name", "empty name")

    local c = buffer:channel(1)
    fail(function() c:channel('r') end,
        "buffer is already a single%-channel view")
    ok(rawequal(c, c:channel(1)), "first channel of a channel is itself")

    buffer:set(1, "AbcdEfghIjklMnop");
    buffer:channel(1):shift(1, pixbuf.SHIFT_CIRCULAR)
    ok(eq(buffer:dump(), "MbcdAfghEjklInop"))
end)
