-- Touch Jog Example
-- Shows how to jog a stepper motor (DRV8825) using 5 touch pads
-- where each touch pad jogs at a difference frequency, slow to fast,
-- when touching the pads. When you release from the pads the jogging
-- is stopped.

-- File hierarchy:
-- touchjog_main.lua
-- - touchjog_touch.lua
-- - touchjog_jog.lua
--     - touchjog_jog_drv8825.lua

-- You need to connect the following pins:
-- Touch pads use pins 2, 12, 13, 14, 15, 27, 33, 32
-- Stepper motor uses pins:
    -- m.pinStep = 4
    -- m.pinDir = 0 
    -- m.pinSleep = 16 
    -- -- Loopback pulse in from m.pinStep (use wire to connect)
    -- m.pinPulseIn = 36 -- 36 (sens_vp)

m = {}
m.jog = require("touchjog_jog")
m.touch = require("touchjog_touch")

m.testFreq = 100
m._isJogging = false

function m.init()

    -- Init jog
    m.jog.init()
    
    -- Init touch pads
    m.touch.init({
    cb=m.onTouch, -- our callback on interrupt
    })
    m.touch.config()
    m.touch.read()
    
    -- Setup the tmr for the callback process
    m._tmr = tmr.create()
    m._tmr:register(150, tmr.ALARM_SEMI, m.onTmr)


end

m._padState = 0 -- 0 means untouched
m._tmr = nil -- will hold the tmr obj

-- We will get a callback every 5ms or so when touched
-- We will reset the tmr each time and after 20ms we will throw
-- untouched event
function m.onTouch(pads)
    
    -- print("state:", m._padState)
    
    if m._padState == 0 then
    -- we just got touched
    m._padState = 1 -- 1 means touched
    m._tmr:start()
    print("Got touch")
    m.jog.jogStart(100)
    else -- m._padState == 1
    -- we will get here on successive callbacks while touched
    -- we get them about every 8ms
    -- so reset the tmr so it doesn't timeout yet
    m._tmr:stop()
    m._tmr:start()
    end
    
    -- pads {2,3,4,5,6}
    local runfreq = 100
    if pads[6] then runfreq = 20
    elseif pads[5] then runfreq = 70
    elseif pads[4] then runfreq = 150
    elseif pads[3] then runfreq = 300
    elseif pads[2] then runfreq = 450
    end
    
    m.jog.setfreq(runfreq,false)
    
end

function m.onTmr()
    -- if we get the timer triggered, it means that we timed out
    -- from touch callbacks, so we presume the touch ended
    m._padState = 0
    print("Got untouch")
    m.jog.jogStop()
end

m.init()

