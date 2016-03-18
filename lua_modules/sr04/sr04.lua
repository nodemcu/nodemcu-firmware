-- Distance measure module for SR04 / SR04T ultrasonic sensor.
-- Single measure or polling is available from 20cm to ~5m.
--
-- Example 1: Read distance once.
--      sr04 = require("sr04")
--      sr04.init(trig_pin=3, echo_pin=4)  -- GPIO 0,2.
--      sr04.get_distance(function(distance) print(distance) end)
--      sr04 = nil
--      package.loaded["sr04"] = nil
--
-- Example 2: Read/print distance periodically
--      sr04 = require("sr04")
--      sr04.init(sample_interval=2000, trig_pin=3, echo_pin=4)  -- Every 2 sec. GPIO 0,2.
--      sr04.poll_distance()  -- Poll in background.
--      -- Do something here, don't tmr.delay!
--      sr04.stop_poll()      -- Enough polling.
--      sr04 = nil
--      package.loaded["sr04"] = nil
--
-- Author: iGrowing
-- License: MIT
--

local moduleName = ...
local M = {}
_G[moduleName] = M 
        
local TID = 0
local SAMPLE_INTERVAL = 150
local TRIG_PIN = 1  -- gpio 5
local ECHO_PIN = 2  -- gpio 4
local _start = 0
local _end = 0

function M.init(timer_id, sample_interval, trig_pin, echo_pin)
-- Set longer sample_interval (1000 or more) for continuous polling --
    -- Initialize
    TID = timer_id or TID
    SAMPLE_INTERVAL = sample_interval or SAMPLE_INTERVAL
    TRIG_PIN = trig_pin or TRIG_PIN  -- gpio 5
    ECHO_PIN = echo_pin or ECHO_PIN  -- gpio 4

    gpio.mode(TRIG_PIN, gpio.OUTPUT)
    gpio.mode(ECHO_PIN, gpio.INT)
    gpio.write(TRIG_PIN, gpio.LOW)
    
    function _trig_cb(level)  -- Internal function
        -- Register in variables the echo start/stop timestamps
        if level == gpio.HIGH then 
            _start = tmr.now() 
        else 
            _end = tmr.now() 
        end
    end
    gpio.trig(ECHO_PIN, "both", _trig_cb)
end

local function _trig()  -- Internal function
    -- Trigger the measure
    gpio.write(TRIG_PIN, gpio.HIGH)
    tmr.delay(10)
    gpio.write(TRIG_PIN, gpio.LOW)
end

local function _calc()  -- Internal function
    local d = _end - _start
    if d > 58 and d < 30000 then -- ~5m max distance
        _end = 0  -- Disable wrong calculation on next attempt.
        return math.floor(d/58) 
    else
        return -1
    end
end

function M.poll_distance()
---  Read and print distance periodically by SAMPLE_INTERVAL.  ---
-- TODO: Rewrite the print with callback: True value comes with SAMPLE_INTERVAL delay.
    tmr.alarm(TID, SAMPLE_INTERVAL, 1, function()
        _trig()
        local d = _calc()
        if d > 1 then print(d) else print("Timeout.") end
    end)
end

function M.stop_poll()
---  Stop distance polling. Clean.  ---
    tmr.stop(TID)
    tmr.unregister(TID)
    collectgarbage()
end

function M.get_distance(callback)
---  Read distance, return value in cm or -1.  ---
    _trig()
    tmr.alarm(TID, SAMPLE_INTERVAL, 0, function()
        callback(_calc())
    end)
end

return M 

