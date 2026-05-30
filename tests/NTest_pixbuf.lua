local N = ...
N = (N or require "NTest")("pixbuf")

local function initBuffer(buf, ...)
  for i,v in ipairs({...}) do
    buf:set(i, v, v*2, v*3, v*4)
  end
  return buf
end

N.test('initialize a buffer', function()
    local buffer = pixbuf.newBuffer(9, 3)
    nok(buffer == nil)
    ok(eq(buffer:size(), 9), "check size")
    ok(eq(buffer:dump(), string.char(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)), "initialize with 0")

    fail(function() pixbuf.newBuffer(9, -1) end, "should be a positive integer")
    fail(function() pixbuf.newBuffer(0, 3) end, "should be a positive integer")
    fail(function() pixbuf.newBuffer(-1, 3) end, "should be a positive integer")
end)

N.test('have correct size', function()
    local buffer = pixbuf.newBuffer(9, 3)
    ok(eq(buffer:size(), 9), "check size")
    buffer = pixbuf.newBuffer(9, 4)
    ok(eq(buffer:size(), 9), "check size")
end)

N.test('fill a buffer with one color', function()
    local buffer = pixbuf.newBuffer(3, 3)
    buffer:fill(1,222,55)
    ok(eq(buffer:dump(), string.char(1,222,55,1,222,55,1,222,55)), "RGB")
    buffer = pixbuf.newBuffer(3, 4)
    buffer:fill(1,222,55,77)
    ok(eq(buffer:dump(), string.char(1,222,55,77,1,222,55,77,1,222,55,77)), "RGBW")
end)

N.test('replace correctly', function()
    local buffer = pixbuf.newBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255))
    ok(eq(buffer:dump(), string.char(3,255,165,33,0,244,12,87,255,0,0,0,0,0,0)), "RGBW")

    buffer = pixbuf.newBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255), 2)
    ok(eq(buffer:dump(), string.char(0,0,0,3,255,165,33,0,244,12,87,255,0,0,0)), "RGBW")

    buffer = pixbuf.newBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255), -5)
    ok(eq(buffer:dump(), string.char(3,255,165,33,0,244,12,87,255,0,0,0,0,0,0)), "RGBW")

    fail(function() buffer:replace(string.char(3,255,165,33,0,244,12,87,255), 4) end,
         "does not fit into destination")
end)

N.test('replace correctly issue #2921', function()
    local buffer = pixbuf.newBuffer(5, 3)
    buffer:replace(string.char(3,255,165,33,0,244,12,87,255), -7)
    ok(eq(buffer:dump(), string.char(3,255,165,33,0,244,12,87,255,0,0,0,0,0,0)), "RGBW")
end)

N.test('replace correctly issue #3702', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    buffer1:set(1, "AAAABBBBCCCCDDDD")
    fail(function() buffer1:replace("XXXX", 5) end,
         "does not fit into destination")
end)

N.test('get/set correctly', function()
    local buffer = pixbuf.newBuffer(3, 4)
    buffer:fill(1,222,55,13)
    ok(eq({buffer:get(2)},{1,222,55,13}))
    buffer:set(2, 4,53,99,0)
    ok(eq({buffer:get(1)},{1,222,55,13}))
    ok(eq({buffer:get(2)},{4,53,99,0}))
    ok(eq(buffer:dump(), string.char(1,222,55,13,4,53,99,0,1,222,55,13)), "RGBW")

    fail(function() buffer:get(0) end,         "index out of range")
    fail(function() buffer:get(4) end,         "index out of range")
    fail(function() buffer:set(0,1,2,3,4) end, "index out of range")
    fail(function() buffer:set(4,1,2,3,4) end, "index out of range")
    fail(function() buffer:set(2,1,2,3) end, "number expected, got no value")
    fail(function() buffer:set(2,1,2,3,4,5) end, "extra values given")
end)

N.test('get/set multiple with string', function()
    -- verify that :set does indeed return its input
    local buffer = pixbuf.newBuffer(4, 3):set(1,"ABCDEF")
    buffer:set(3,"LMNOPQ")
    ok(eq(buffer:dump(), "ABCDEFLMNOPQ"))

    fail(function() buffer:set(4,"AAAAAA") end, "string size will exceed strip length")
    fail(function() buffer:set(2,"AAAAA") end, "string does not contain whole LEDs")
end)

N.test('fade correctly', function()
    local buffer = pixbuf.newBuffer(1, 3)
    buffer:fill(1,222,55)
    buffer:fade(2)
    ok(buffer:dump() == string.char(0,111,27), "RGB")
    buffer:fill(1,222,55)
    buffer:fade(3, pixbuf.FADE_OUT)
    ok(buffer:dump() == string.char(0,math.floor(222/3),math.floor(55/3)), "RGB")
    buffer:fill(1,222,55)
    buffer:fade(3, pixbuf.FADE_IN)
    ok(buffer:dump() == string.char(3,255,165), "RGB")
    buffer = pixbuf.newBuffer(1, 4)
    buffer:fill(1,222,55, 77)
    buffer:fade(2, pixbuf.FADE_OUT)
    ok(eq(buffer:dump(), string.char(0,111,27,38)), "RGBW")
end)

N.test('mix correctly issue #1736', function()
    local buffer1 = pixbuf.newBuffer(1, 3)
    local buffer2 = pixbuf.newBuffer(1, 3)
    buffer1:fill(10,22,54)
    buffer2:fill(10,27,55)
    buffer1:mix(256/8*7,buffer1,256/8,buffer2)
    ok(eq({buffer1:get(1)}, {10,23,54}))
end)

N.test('mix saturation correctly ', function()
    local buffer1 = pixbuf.newBuffer(1, 3)
    local buffer2 = pixbuf.newBuffer(1, 3)

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
    local buffer = pixbuf.newBuffer(2, 4)
    buffer:fill(10,22,54,234)
    ok(eq(buffer:power(), 2*(10+22+54+234)))
end)

N.test('shift LOGICAL', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    local buffer2 = pixbuf.newBuffer(4, 4)

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,0,7,8)
    ok(buffer1 ~= buffer2, "disequality pre shift")
    buffer1:shift(2)
    ok(buffer1 == buffer2, "shift right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12,0,0)
    buffer1:shift(-2)
    ok(buffer1 == buffer2, "shift left")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,0,8,12)
    buffer1:shift(1, nil, 2,3)
    ok(buffer1 == buffer2, "shift middle right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,9,0,12)
    buffer1:shift(-1, nil, 2,3)
    ok(buffer1 == buffer2, "shift middle left")

    -- bounds checks, handle gracefully as string:sub does
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,8,9,12,0)
    buffer1:shift(-1, pixbuf.SHIFT_LOGICAL, 0,5)
    ok(buffer1 == buffer2, "shift left out of bound")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,7,8,9)
    buffer1:shift(1, pixbuf.SHIFT_LOGICAL, 0,5)
    ok(buffer1 == buffer2, "shift right out of bound")

end)

N.test('shift LOGICAL issue #2946', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    local buffer2 = pixbuf.newBuffer(4, 4)

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,0,0,0)
    buffer1:shift(4)
    ok(buffer1 == buffer2, "shift all right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,0,0,0,0)
    buffer1:shift(-4)
    ok(buffer1 == buffer2, "shift all left")

    fail(function() buffer1:shift(10) end, "shifting more elements than buffer size")
    fail(function() buffer1:shift(-6) end, "shifting more elements than buffer size")
end)

N.test('shift LOGICAL issue #3702', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    fail(function() buffer1:shift(1, pixbuf.SHIFT_LOGICAL, 5, 5) end,
         "end position must be >= start")
end)

N.test('shift CIRCULAR', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    local buffer2 = pixbuf.newBuffer(4, 4)

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12,7,8)
    buffer1:shift(2, pixbuf.SHIFT_CIRCULAR)
    ok(buffer1 == buffer2, "shift right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12,7,8)
    buffer1:shift(-2, pixbuf.SHIFT_CIRCULAR)
    ok(buffer1 == buffer2, "shift left")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,9,8,12)
    buffer1:shift(1, pixbuf.SHIFT_CIRCULAR, 2,3)
    ok(buffer1 == buffer2, "shift middle right")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,9,8,12)
    buffer1:shift(-1, pixbuf.SHIFT_CIRCULAR, 2,3)
    ok(buffer1 == buffer2, "shift middle left")

    -- bounds checks, handle gracefully as string:sub does
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,8,9,12,7)
    buffer1:shift(-1, pixbuf.SHIFT_CIRCULAR, 0,5)
    ok(buffer1 == buffer2, "shift left out of bound")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,12,7,8,9)
    buffer1:shift(1, pixbuf.SHIFT_CIRCULAR, 0,5)
    ok(buffer1 == buffer2, "shift right out of bound")

    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,12,7,8,9)
    buffer1:shift(1, pixbuf.SHIFT_CIRCULAR, -12,12)
    ok(buffer1 == buffer2, "shift right way out of bound")

end)

N.test('sub', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    buffer1 = buffer1:sub(4,3)
    ok(eq(buffer1:size(), 0), "sub empty")

    local buffer2 = pixbuf.newBuffer(2, 4)
    buffer1 = pixbuf.newBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,9,12)
    buffer1 = buffer1:sub(3,4)
    ok(buffer1 == buffer2, "sub")

    buffer1 = pixbuf.newBuffer(4, 4)
    buffer2 = pixbuf.newBuffer(4, 4)
    initBuffer(buffer1,7,8,9,12)
    initBuffer(buffer2,7,8,9,12)
    buffer1 = buffer1:sub(-12,33)
    ok(buffer1 == buffer2, "out of bounds")
end)

N.test('map', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    buffer1:fill(65,66,67,68)

    buffer1:map(function(a,b,c,d) return b,a,c,d end)
    ok(eq("BACDBACDBACDBACD", buffer1:dump()), "swizzle")

    local buffer2 = pixbuf.newBuffer(4, 1)
    buffer2:map(function(b,a,c,d) return c end, buffer1) -- luacheck: ignore
    ok(eq("CCCC", buffer2:dump()), "projection")

    local buffer3 = pixbuf.newBuffer(4, 3)
    buffer3:map(function(b,a,c,d) return a,b,d end, buffer1) -- luacheck: ignore
    ok(eq("ABDABDABDABD", buffer3:dump()), "projection 2")

    buffer1:fill(70,71,72,73)
    buffer1:map(function(c,a,b,d) return a,b,c,d end, buffer2, nil, nil, buffer3)
    ok(eq("ABCDABCDABCDABCD", buffer1:dump()), "zip")

    buffer1 = pixbuf.newBuffer(2, 4)
    buffer1:fill(70,71,72,73)
    buffer2:set(1,"ABCD")
    buffer3:set(1,"EFGHIJKLM")
    buffer1:map(function(c,a,b,d) return a,b,c,d end, buffer2, 1, 2, buffer3, 2)
    ok(eq("HIAJKLBM", buffer1:dump()), "partial zip")
end)

N.test('map issue #3702', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
    local buffer2 = pixbuf.newBuffer(1, 4)
    local f = function(a, b, c, d) return a+10, b+10, c+10, d+10 end
    buffer1:fill(65,66,67,68)
    buffer2:map(f, buffer1, -1, -1) -- just the last pixel
    ok(eq("KLMN", buffer2:dump()), "map last pixel only")
end)

N.test('sub boundaries behave like string.sub', function()
    local buffer1 = pixbuf.newBuffer(4, 4)
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

--[[
pixbuf.buffer:__concat()
--]]
