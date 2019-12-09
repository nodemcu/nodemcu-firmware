require 'mispec'

local buffer, buffer1, buffer2

describe('WS2812_effects', function(it)

    it:should('set_speed', function()
        buffer = ws2812.newBuffer(9, 3)
        ws2812_effects.init(buffer)

        ws2812_effects.set_speed(0)
        ws2812_effects.set_speed(255)

        failwith("should be", ws2812_effects.set_speed, -1)
        failwith("should be", ws2812_effects.set_speed, 256)
    end)

    it:should('set_brightness', function()
        buffer = ws2812.newBuffer(9, 3)
        ws2812_effects.init(buffer)

        ws2812_effects.set_brightness(0)
        ws2812_effects.set_brightness(255)

        failwith("should be", ws2812_effects.set_brightness, -1)
        failwith("should be", ws2812_effects.set_brightness, 256)
    end)

end)

mispec.run()
