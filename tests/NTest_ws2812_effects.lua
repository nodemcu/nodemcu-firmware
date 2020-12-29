local N = ...
N = (N or require "NTest")("ws2812_effects")

local buffer, buffer1, buffer2


N.test('set_speed', function()
  buffer = ws2812.newBuffer(9, 3)
  ws2812_effects.init(buffer)

  ws2812_effects.set_speed(0)
  ws2812_effects.set_speed(255)

  fail(function() ws2812_effects.set_speed(-1) end, "should be")
  fail(function() ws2812_effects.set_speed(256) end, "should be")
end)

N.test('set_brightness', function()
  buffer = ws2812.newBuffer(9, 3)
  ws2812_effects.init(buffer)

  ws2812_effects.set_brightness(0)
  ws2812_effects.set_brightness(255)

  fail(function() ws2812_effects.set_brightness(-1) end, "should be")
  fail(function() ws2812_effects.set_brightness(256) end, "should be")
end)
