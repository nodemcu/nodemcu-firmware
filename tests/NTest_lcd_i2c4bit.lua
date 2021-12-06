-- Run LiquidCrystal through some basic tests.  Requires `liquidcrystal.lua`
-- and `l2-i2c4bit.lua` available available to `require`.
--
-- This file ought to be named "NTest_liquidcrystal_i2c4bit" or something,
-- but it has its current name due to our default SPIFFS filename length limit.

local N = ...
N = (N or require "NTest")("liquidcrystal-i2c4bit")

local metalcd
local metaback
local backend
local lcd

collectgarbage()
print("HEAP init", node.heap())

metalcd = require "liquidcrystal"
collectgarbage() print("HEAP constructor imported ", node.heap())

metaback = require "lc-i2c4bit"
collectgarbage() print("HEAP backend imported ", node.heap())

backend = metaback({
 address = 0x27,
 id  = 0,
 speed = i2c.SLOW,
 sda = 2,
 scl = 1,
})
collectgarbage() print("HEAP backend built", node.heap())

lcd = metalcd(backend, false, true, 20)
collectgarbage() print("HEAP lcd built", node.heap())

print("waiting for LCD to be unbusy before testing...")
while lcd:busy() do end

N.test("custom character", function()
  local glyph = { 0x1F, 0x15, 0x1B, 0x15, 0x1F, 0x10, 0x10, 0x0 }
  lcd:customChar(0, glyph)
  ok(eq(glyph,lcd:readCustom(0)), "read back")
end)

N.test("draw and readback", function()
  lcd:cursorMove(0)
  lcd:write("abc")
  lcd:cursorMove(10,1)
  lcd:write("de")
  lcd:cursorMove(10,2)
  lcd:write("fg")
  lcd:cursorMove(12,3)
  lcd:write("hi\000")
  lcd:cursorMove(18,4)
  lcd:write("jk")

  lcd:home()           ok(eq(0x61, lcd:read()), "read back 'a'")
                       ok(eq(0x62, lcd:read()), "read back 'b'")
  lcd:cursorMove(11,1) ok(eq(0x65, lcd:read()), "read back 'e'")
  lcd:cursorMove(11,2) ok(eq(0x67, lcd:read()), "read back 'g'")
  lcd:cursorMove(13,3) ok(eq(0x69, lcd:read()), "read back 'i'")
  lcd:cursorMove(14,3) ok(eq(0x00, lcd:read()), "read back  0" )
  lcd:cursorMove(19,4) ok(eq(0x6B, lcd:read()), "read back 'k'")

end)

N.test("update home", function()
  lcd:home() lcd:write("l")
  lcd:home() ok(eq(0x6C, lcd:read()))
end)

N.testasync("clear", function(next)
  -- clear and poll busy
  lcd:clear()
  tmr.create():alarm(5, tmr.ALARM_SEMI, function(tp)
    if lcd:busy() then tp:start() else next() end
  end)
  lcd:home() -- work around busy polling incrementing position (XXX)
  ok(eq(0x20, lcd:read()), "is space")
  ok(eq(1, lcd:position())) -- having just read 1 from home, we should be at 1
end)


