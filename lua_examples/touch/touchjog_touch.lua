-- Touch module for 5 pads
-- Configures 5 pads at 90% threshold and gives callback

-- Using pins 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 
-- 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32

local m = {}

-- Touch uses pins 2, 12, 13, 14, 15, 27, 33, 32
m.pads = {2,3,4,5,6} -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32

-- Public object for touchpad
m.tp = nil

m._cb = nil
m._isInitted = false

-- Pass in tbl
-- @param cb  This is your callback when pads touched
function m.init(tbl)
  
  if m._isInitted then
    print("Already initted")
    return
  end
  
  if tbl ~= nil then
    if tbl.cb ~= nil then m._cb = tbl.cb end 
  end
  
  m._tp = touch.create({
    pad = m.pads, -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
    cb = m._cb, -- Callback will get Lua table of pads/bool(true) that were touched.
    intrInitAtStart = false, -- Turn on interrupt at start. Default to true. Set to false in case you want to config first. Turn it on later with tp:intrEnable()
    thres = 0, -- Start with thres 0 so no callbacks, then set thres later with tp:setThres(padNum, thres)
    thresTrigger = touch.TOUCH_TRIGGER_BELOW, -- Touch interrupt if counter value is below or above threshold. TOUCH_TRIGGER_BELOW or TOUCH_TRIGGER_ABOVE 
    lvolt = touch.TOUCH_LVOLT_0V5, -- Low ref voltage TOUCH_LVOLT_0V4, TOUCH_LVOLT_0V5, TOUCH_LVOLT_0V6, TOUCH_LVOLT_0V7 
    hvolt = touch.TOUCH_HVOLT_2V7, -- High ref voltage TOUCH_HVOLT_2V4, TOUCH_HVOLT_2V5, TOUCH_HVOLT_2V6, TOUCH_HVOLT_2V7
    atten = touch.TOUCH_HVOLT_ATTEN_1V, -- High ref atten TOUCH_HVOLT_ATTEN_0V, TOUCH_HVOLT_ATTEN_0V5, TOUCH_HVOLT_ATTEN_1V, TOUCH_HVOLT_ATTEN_1V5 
    isDebug = true
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
  print("Configuring trigger to:")
  print("Pad", "Val", "Thres")
  for key,value in pairs(raw) do 
    -- reduce thres to 90% of untouched counter value
    local thres = math.floor(raw[key] * 0.9)
    m._tp:setThres(key, thres)
    print(key, raw[key], thres)
  end

  m._tp:intrEnable()
end

return m
