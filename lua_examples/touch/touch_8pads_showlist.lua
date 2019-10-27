-- Touch sensor for 8 pads
-- Set threshold to 30% of untouched state
-- Print padNum list per callback

-- To use:
-- tpad = require("touch_8pads_showlist")
-- tpad.init({isDebug=false})
-- tpad.config()

local m = {}

m.pad = {2,3,4,5,6,7,8,9} -- 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32

m._tp = nil -- will hold the touchpad obj
m._isDebug = true

-- We will get a callback every 8ms or so when touched
function m.onTouch(pads)

    print(m.pconcat(pads))
  
end

function m.pconcat(tab)
    local ctab = {}
    local n = 1
    for k, v in pairs(tab) do
        ctab[n] = k
        n = n + 1
    end
    return table.concat(ctab, ",")
end

function m.init(tbl)

    if (tbl ~= nil) then
        if (tbl.isDebug ~= nil) then m._isDebug = tbl.isDebug end
    end

    m._tp = touch.create({
        pad = m.pad, -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
        cb = m.onTouch, -- Callback will get Lua table of pads/bool(true) that were touched.
        intrInitAtStart = false, -- Turn on interrupt at start. Default to true. Set to false to config first. Turn on later with tp:intrEnable()
        thresTrigger = touch.TOUCH_TRIGGER_BELOW, -- TOUCH_TRIGGER_BELOW or TOUCH_TRIGGER_ABOVE. Touch interrupt happens if counter is below or above.  
        lvolt = touch.TOUCH_LVOLT_0V5, -- Low ref voltage TOUCH_LVOLT_0V4, TOUCH_LVOLT_0V5, TOUCH_LVOLT_0V6, TOUCH_LVOLT_0V7 
        hvolt = touch.TOUCH_HVOLT_2V7, -- High ref voltage TOUCH_HVOLT_2V4, TOUCH_HVOLT_2V5, TOUCH_HVOLT_2V6, TOUCH_HVOLT_2V7
        atten = touch.TOUCH_HVOLT_ATTEN_1V, -- TOUCH_HVOLT_ATTEN_0V, TOUCH_HVOLT_ATTEN_0V5, TOUCH_HVOLT_ATTEN_1V, TOUCH_HVOLT_ATTEN_1V5 
        isDebug = m._isDebug
    })

end

function m.read()
    local raw = m._tp:read()
    print("Pad", "Val")
    for key,value in pairs(raw) do 
        print(key,value) 
    end
end

function m.config()
    local raw = m._tp:read()

    if (m._isDebug) then
        print("Configuring...")
        print("Pad", "Base", "Thres")
    end

    for key,value in pairs(raw) do 
        if key ~= nil then
            -- reduce by 30%
            local thres = raw[key] - math.floor(raw[key] * 0.3)
            m._tp:setThres(key, thres)
            if (m._isDebug) then print(key, value, thres) end
        end
    end
    m._tp:intrEnable()
    if (m._isDebug) then print("You can now touch the sensors") end
end

-- m.init()
-- m.read()
-- m.config()

return m
