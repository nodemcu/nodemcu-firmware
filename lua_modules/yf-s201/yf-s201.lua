-- Water flow meter module for YF-S201 sensor.
-- Returns consumed water in liters since previous reading.
-- Resets the counter on read.
--
-- Example 1: Read water consumption.
--      yf = require("yf-s201")
--      yf.init(pin=3)  -- GPIO 0
--      yf.get_liters()
--      yf.get_ticks()
--      -- Do something, don't unload the module.
--      -- Unloaded module doesn't counter water consumption.
--      yf = nil
--      package.loaded["yf-s201"] = nil
--
-- Author: iGrowing
-- License: MIT
--
-- 8.2 ticks per 1 liter
-- 8.2 Up/Down triggers in 1 sec => 10 ticks in 1220 msec

local moduleName = ... 
local M = {}
_G[moduleName] = M 

local _pin=5   -- GPIO 14
local counter = 0

function M.init(pin)
    pin = pin or _pin
    gpio.mode(pin, gpio.INT, gpio.PULLUP)
    gpio.trig(pin, 'down', function() counter = counter + 1 end)
end

function M.get_ticks()
    t = counter
    counter = 0
    return t
end

function M.get_liters()
    return M.get_ticks() * 10 / 82  -- ticks / 8.2 (for int-based NodeMCU)
end

return M 