# Touch Sensor Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-07-20 | [ChiliPeppr](https://github.com/chilipeppr) | John Lauer | [touch.c](../../components/modules/touch.c)|

The touch sensor module enables you to easily interact with ESP32's built-in 10 touch sensors. The touch pad sensing process is under the control of a hardware-implemented finite-state machine (FSM) which is initiated by software (polling mode) or a dedicated hardware timer (interrupt mode). By using the interrupt mode you can offload the sensing away from the main CPU. There are several examples in the docs below on how to implement your code.

For further information please refer to the ESP-IDF docs for Touch Sensor
https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/touch_pad.html

Click the YouTube video below for a tutorial on how to use this touch library including sample touch sensors, how to set your touch counter thresholds, and what the touch counters look like in the touched/untouched state of a sensor pad.

[![YouTube Touch Tutorial](../img/touch_tutorial.jpg "Walkthrough video")](https://youtu.be/6BxNFh3GaRA)

The touch sensors are on the following GPIO pins

| Touch Pad Number | GPIO |   | Touch Pad Number | GPIO | 
| :--------------- | :--- | -  | :--------------- | :--- |
| 0 | GPIO4 |    | 5 | GPIO12 |
| 1 | GPIO0 |    | 6 | GPIO14 |
| 2 | GPIO2 |    | 7 | GPIO27 |
| 3 | GPIO15 |   | 8 | GPIO33 |
| 4 | GPIO13 |   | 9 | GPIO32 |

#### Example Lua Code

Example code showing how to configure 8 pads.
- Main run file [touch_8pads_showlist_test.lua](../../lua_examples/touch/touch_8pads_showlist_test.lua)
- Library [touch_8pads_showlist.lua](../../lua_examples/touch/touch_8pads_showlist.lua)

Example code showing how to use 5 touch pads to jog a stepper motor at different frequencies depending on which pad is touched:
- Main run file [touchjog_main.lua](../../lua_examples/touch/touchjog_main.lua)
- Library [touchjog_touch.lua](../../lua_examples/touch/touchjog_touch.lua)
- Library [touchjog_jog.lua](../../lua_examples/touch/touchjog_jog.lua)
- Library [touchjog_jog_drv8825.lua](../../lua_examples/touch/touchjog_jog_drv8825.lua)

## touch.create()

Create the touch sensor object. You must call this method first. Only one touch object may be created since most settings on the touch driver are global in nature such as threshold trigger mode, interrupt callbacks, and reference voltages.

#### Syntax
```lua
tp = touch.create({
  pad = 0 || {0,1,2,3,4,5,6,7,8,9}, -- 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
  cb = yourFunc, -- Callback on interrupt
  intrInitAtStart = true || false, -- Turn on/off interrupt at start.
  thres = 0..65536, -- All pads set to this thres. 
  thresTrigger = touch.TOUCH_TRIGGER_BELOW || touch.TOUCH_TRIGGER_ABOVE,
  filterMs = 0..4294967295 -- Filter is only available in polling mode. 
  lvolt = touch.TOUCH_LVOLT_0V4 || touch.TOUCH_LVOLT_0V5 || touch.TOUCH_LVOLT_0V6 || touch.TOUCH_LVOLT_0V7,
  hvolt = touch.TOUCH_HVOLT_2V4 || touch.TOUCH_HVOLT_2V5 || touch.TOUCH_HVOLT_2V6 || touch.TOUCH_HVOLT_2V7, 
  atten = touch.TOUCH_HVOLT_ATTEN_0V || touch.TOUCH_HVOLT_ATTEN_0V5 || touch.TOUCH_HVOLT_ATTEN_1V || touch.TOUCH_HVOLT_ATTEN_1V5 
  isDebug = true || false
})
```

#### Parameters
List of values for configuration table:

- `pad` Required. padNum || {table of padNums}. Specify one pad like `pad = 4`, or provide a table list of pads. For example use `pad = {0,1,2,3,4,5,6,7,8,9}` to specify all pads. Pads allowed are 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32.
- `cb` Optional. Your Lua method that gets called on touch event. `myfunction(pads)` will be called where `pads` is a table of pads that were touched. The key is the pad number while the value is true, i.e. `pads = {[2]=true, [3]=true}` if pad 2 and 3 were both touched at the same time. When you specify a callback, interrupt mode is automatically turned on. You can specify `intrInitAtState = false` to manually turn on the interrupt later. If no callback is provided or nil is specified, then polling mode is active where you must call `tp:read()` to get the touch pad values.
- `intrInitAtStart` Optional. Defaults to true. Turn on interrupt at start. Set to false to if you want to configure the touch sensors first and then manually turn on interrupts later with `tp:intrEnable()`.
- `thres` Optional. Defaults to 0. Range is 0 to 65536. Provide a threshold value to be set on all pads specified in the `pad` parameter. Typically you will set thresholds per pad since pad size/shape/wire legnth influences the counter value per pad and thus your threshold is usually differnt per pad. You can set thres later per pad with `tp:setThres(padNum, thres)`.
- `thresTrigger` Optional. Defaults to touch.TOUCH_TRIGGER_BELOW.
    - touch.TOUCH_TRIGGER_BELOW
    - touch.TOUCH_TRIGGER_ABOVE
- `filterMs` Optional. Range is 0 to 4294967295 milliseconds. Used in polling mode only (if you provide a callback polling mode is disabled). Will filter noise for this many ms to give more consistent counter results. When filterMs is specified you will receive a 2nd return value in the `raw, filter = tp:read()` call with the filtered values in a Lua table.
- `lvolt` Optional. Low reference voltage 
    - touch.TOUCH_LVOLT_0V4
    - touch.TOUCH_LVOLT_0V5
    - touch.TOUCH_LVOLT_0V6
    - touch.TOUCH_LVOLT_0V7 
- `hvolt` Optional. High reference voltage 
    - touch.TOUCH_HVOLT_2V4
    - touch.TOUCH_HVOLT_2V5
    - touch.TOUCH_HVOLT_2V6
    - touch.TOUCH_HVOLT_2V7
- `atten` Optional. High reference voltage attenuation 
    - touch.TOUCH_HVOLT_ATTEN_0V
    - touch.TOUCH_HVOLT_ATTEN_0V5
    - touch.TOUCH_HVOLT_ATTEN_1V
    - touch.TOUCH_HVOLT_ATTEN_1V5
- `isDebug` Optional. Defaults to false. Set to true to get debug information during development. The info returned while debug is on can be very helpful in understanding polling vs interrupts, configuration, and threshold settings. Set to false during production.

#### Returns
`tp` touch object

#### Example 1 - Polling
```lua
-- Touch sensor with 5 touch pads for polling counter state

tp = touch.create({
  pad = {0,1,2,3,4}, -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
  lvolt = touch.TOUCH_LVOLT_0V5, -- Low ref voltage TOUCH_LVOLT_0V4, TOUCH_LVOLT_0V5, TOUCH_LVOLT_0V6, TOUCH_LVOLT_0V7 
  hvolt = touch.TOUCH_HVOLT_2V7, -- High ref voltage TOUCH_HVOLT_2V4, TOUCH_HVOLT_2V5, TOUCH_HVOLT_2V6, TOUCH_HVOLT_2V7
  atten = touch.TOUCH_HVOLT_ATTEN_1V, -- High ref attenuation TOUCH_HVOLT_ATTEN_0V, TOUCH_HVOLT_ATTEN_0V5, TOUCH_HVOLT_ATTEN_1V, TOUCH_HVOLT_ATTEN_1V5 
  isDebug = true
})

function read()
  raw = tp:read()
  print("Pad", "RawVal")
  for key, value in pairs(raw) do
    print(key, value)
  end
end

read()
```

#### Example 2 - Polling with Filtering
```lua
-- Touch sensor with 3 touch pads for polling with filtering

tp = touch.create({
  pad = {4,5,6}, -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
  filterMs = 20, -- Polling mode only. Will filter noise for this many ms to give more consistent counter.
  lvolt = touch.TOUCH_LVOLT_0V5, -- Low ref voltage TOUCH_LVOLT_0V4, TOUCH_LVOLT_0V5, TOUCH_LVOLT_0V6, TOUCH_LVOLT_0V7 
  hvolt = touch.TOUCH_HVOLT_2V7, -- High ref voltage TOUCH_HVOLT_2V4, TOUCH_HVOLT_2V5, TOUCH_HVOLT_2V6, TOUCH_HVOLT_2V7
  atten = touch.TOUCH_HVOLT_ATTEN_1V, -- High ref attenuation TOUCH_HVOLT_ATTEN_0V, TOUCH_HVOLT_ATTEN_0V5, TOUCH_HVOLT_ATTEN_1V, TOUCH_HVOLT_ATTEN_1V5 
  isDebug = true
})

-- You will get 2 parameters from tp:read() when filterMs is specified in create()
function read()
  raw, filter = tp:read()
  -- Use sjson to make pretty output
  print("Touch raw:", sjson.encode(raw))
  print("Touch filt:", sjson.encode(filter))
end

-- Filtered vals will be similar to raw vals on first read.
read()
-- Read a second later to see how filtered vals are more stable.
tmr.create():alarm(1000, tmr.ALARM_SINGLE, read)
```

#### Example 3 - Interrupt Touch / Untouch
```lua
-- Touch sensor with 1 pad for touch / untouch using threshold trigger mode
-- Swap TOUCH_TRIGGER_BELOW / TOUCH_TRIGGER_ABOVE on each callback
-- NOTE: Can only use this technique with 1 pad

pad = 6 -- 6=GPIO14

padState = 0 -- 0 means untouched
tp = nil -- Holds our touch sensor object

function onTouch(pads)
    
  if padState == 0 then
    -- we just got touched
    -- swap trigger mode to TOUCH_TRIGGER_ABOVE so we get interrupt
    -- on untouch
    padState = 1
    tp:setTriggerMode(touch.TOUCH_TRIGGER_ABOVE)
    print("Touch")
  else
    -- we just got untouched
    -- swap trigger mode to TOUCH_TRIGGER_BELOW so we get interrupt
    -- on touch
    padState = 0
    tp:setTriggerMode(touch.TOUCH_TRIGGER_BELOW)
    print("Untouch")
  end
  
end

function init()
  tp = touch.create({
    pad = pad, -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
    cb = onTouch, -- Callback will get Lua table of pads/bool(true) that were touched.
    intrInitAtStart = false, -- Turn on interrupt at start. Default to true. Set to false to config first. Turn on later with tp:intrEnable()
    thresTrigger = touch.TOUCH_TRIGGER_BELOW, -- TOUCH_TRIGGER_BELOW or TOUCH_TRIGGER_ABOVE. Touch interrupt happens if ctr value is below or above.  
    lvolt = touch.TOUCH_LVOLT_0V5, -- Low ref voltage TOUCH_LVOLT_0V4, TOUCH_LVOLT_0V5, TOUCH_LVOLT_0V6, TOUCH_LVOLT_0V7 
    hvolt = touch.TOUCH_HVOLT_2V7, -- High ref voltage TOUCH_HVOLT_2V4, TOUCH_HVOLT_2V5, TOUCH_HVOLT_2V6, TOUCH_HVOLT_2V7
    atten = touch.TOUCH_HVOLT_ATTEN_1V, -- High ref attenuation TOUCH_HVOLT_ATTEN_0V, TOUCH_HVOLT_ATTEN_0V5, TOUCH_HVOLT_ATTEN_1V, TOUCH_HVOLT_ATTEN_1V5 
    isDebug = true
  })
  
end

function read()
  local raw = tp:read()
  print("Pad", "Val")
  for key,value in pairs(raw) do 
    print(key,value) 
  end
end

-- Do not touch during config
function config()
  local raw = tp:read()
  -- set threshold to 20% of baseline read state
  local thres = raw[pad] - math.floor(raw[pad] * 0.2)
  tp:setThres(pad, thres)
  print("Pad is at " .. raw[pad] .. " when not touched")
  print("Will trigger at thres: " .. thres)
  tp:intrEnable()
end

init()
read()
config()
```

#### Example 4 - Interrupt Touch / Untouch with Timer
```lua
-- Touch sensor with 1 pad for touch / untouch using timer
-- Shows how to detect a touch and then an untouch

m = {}

m.pad = {8} -- 8=GPIO33

m._tp = nil -- will hold the touchpad obj
m._padState = 0 -- 0 means untouched
m._tmr = nil -- will hold the tmr obj
m._tmrWait = 50 -- ms to wait for no callback to indicate untouch

-- We will get a callback every 8ms or so when touched
-- We will reset the tmr each time and after 50ms will presume untouched event
function m.onTouch(pads)
  
  if m._padState == 0 then
    -- we just got touched
    m._padState = 1 -- 1 means touched
    m._tmr:start()
    print("Touch")
  else -- m._padState == 1
    -- we will get here on successive callbacks while touched
    -- we get them about every 8ms
    -- reset the tmr so it doesn't timeout yet
    m._tmr:stop()
    m._tmr:start()
  end
    
end

function m.onTmr()
  -- if we get the timer triggered, it means that we timed out
  -- from touch callbacks, so we presume the touch ended
  m._padState = 0
  print("Untouch")
end

function m.init()
  m._tp = touch.create({
    pad = m.pad, -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
    cb = m.onTouch, -- Callback will get Lua table of pads/bool(true) that were touched.
    intrInitAtStart = false, -- Turn on interrupt at start. Default to true. Set to false in case you want to config first. Turn it on later with tp:intrEnable()
    thresTrigger = touch.TOUCH_TRIGGER_BELOW, -- TOUCH_TRIGGER_BELOW or TOUCH_TRIGGER_ABOVE. Touch interrupt will happen if counter value is below or above threshold.  
    lvolt = touch.TOUCH_LVOLT_0V5, -- Low ref voltage TOUCH_LVOLT_0V4, TOUCH_LVOLT_0V5, TOUCH_LVOLT_0V6, TOUCH_LVOLT_0V7 
    hvolt = touch.TOUCH_HVOLT_2V7, -- High ref voltage TOUCH_HVOLT_2V4, TOUCH_HVOLT_2V5, TOUCH_HVOLT_2V6, TOUCH_HVOLT_2V7
    atten = touch.TOUCH_HVOLT_ATTEN_1V, -- High ref attenuation TOUCH_HVOLT_ATTEN_0V, TOUCH_HVOLT_ATTEN_0V5, TOUCH_HVOLT_ATTEN_1V, TOUCH_HVOLT_ATTEN_1V5 
    isDebug = true
  })
  
  -- Setup the tmr for the callback process
  m._tmr = tmr.create()
  m._tmr:register(m._tmrWait, tmr.ALARM_SEMI, m.onTmr)
  
end

function m.read()
  local raw = m._tp:read()
  print("Pad", "Val")
  for key,value in pairs(raw) do 
    print(key,value) 
  end
end

-- Do not touch pad during config
function m.config()
  local raw = m._tp:read()
  -- set threshold to 20% of baseline read state
  local thres = raw[m.pad] - math.floor(raw[m.pad] * 0.2)
  m._tp:setThres(m.pad, thres)
  print("Pad is at ", raw[m.pad], " as baseline")
  print("Will trigger at thres: ", thres)
  m._tp:intrEnable()
end

m.init()
m.read()
m.config()
```

## touchObj:read()

Read the touch sensor counter values for all pads configured in `touch.create()` method.

#### Syntax
`raw, filter = tp:read()`

#### Parameters
None

#### Returns
- `raw` Lua table of touch sensor counter values per pad
- `filter` A 2nd Lua table of touch sensor filtered counter values per pad is returned if `filterMs` is specified during the touch.create() method.

#### Example 1 - Raw
```lua
raw = tp:read()
print("Pad", "Val")
for key,value in pairs(raw) do 
  print(key,value) 
end
```

#### Example 2 - Raw / Filter
```lua
-- You get a filter Lua table if you specified filterMs in touch.create()
raw, filter = tp:read()
print("Pad", "Raw", "Filt")
print("---", "---", "----")
for key,value in pairs(raw) do 
  print(key,value,filter[key]) 
end
```

## touchObj:setThres(padNum, thresVal)

Set touch sensor interrupt threshold per pad. The threshold only matters if you are in interrupt mode, which only activates if you specify a callback in the `touch.create()` configuration.

#### Syntax
`tp:setThres(padNum, thresVal)`

#### Parameters
- `padNum` Required. One pad number can be specified here. If you did multiple pads you must call this per pad.
- `thresVal` Required. The threshold value to set for the pad interrupt trigger. If you set touch.TOUCH_TRIGGER_BELOW then the interrupt occurs when the touch counter goes below this threshold value, or vice versa for touch.TOUCH_TRIGGER_ABOVE.

#### Returns
`nil`

#### Example 1
```lua
-- Set threshold for pad 2 where baseline counter is around 800 and 
-- when touched is around 200, so trigger at mid-point around 500
tp:setThres(2, 500)
```
#### Example 2
```lua
-- Configure by reading baseline, then setting threshold to 30% below base
local raw = tp:read()
print("Pad", "Base", "Thres")
for key,value in pairs(raw) do 
  if key ~= nil then
    -- reduce by 30%
    local thres = raw[key] - math.floor(raw[key] * 0.3)
    tp:setThres(key, thres)
    print(key, value, thres) 
  end
end
-- Now enable interrupts since our thresholds are correct
tp:intrEnable()
```

## touchObj:setTriggerMode()

Set the trigger mode globally for all touch pads. The trigger mode only matters in interrupt mode where you can tell the hardware to give you an interrupt if the counter on the pad falls above or below the threshold you specify. 

#### Syntax
`tp:setTriggerMode(mode)`

#### Parameters
- `mode` Required. touch.TOUCH_TRIGGER_BELOW or touch.TOUCH_TRIGGER_ABOVE can be specified. If your pad's baseline counter value is around 600 when not touched, and sits around 300 when touched, then you would set your threshold around 450. If you set touch.TOUCH_TRIGGER_BELOW then when the counter drops to 300 it would fall BELOW the threshold of 450, thus triggering the interrupt. This process works in the reverse for touch.TOUCH_TRIGGER_ABOVE.

#### Returns
`nil`

#### Example 1
```lua
-- Trigger callback when pad counter goes above threshold value
tp:setTriggerMode(touch.TOUCH_TRIGGER_ABOVE)
-- Trigger callback when pad counter goes below threshold value
tp:setTriggerMode(touch.TOUCH_TRIGGER_BELOW)
```

#### Example 2
```lua
-- Configure touch hardware to callback on TOUCH_TRIGGER_BELOW during touch.create()
-- Then on first callback swap to TOUCH_TRIGGER_ABOVE when detecting touch
-- Then will get 2nd callback on untouch due to mode change
-- This only works with 1 pad since trigger mode is global for all pads

padState = 0 -- Set start padState

function onTouch(pads)
    
  if padState == 0 then
    -- we just got touched
    -- swap trigger mode to TOUCH_TRIGGER_ABOVE so we get interrupt
    -- on untouch
    padState = 1
    tp:setTriggerMode(touch.TOUCH_TRIGGER_ABOVE)
    print("Got touch")
  else
    -- we just got untouched
    -- swap trigger mode to TOUCH_TRIGGER_BELOW so we get interrupt
    -- on touch
    padState = 0
    tp:setTriggerMode(touch.TOUCH_TRIGGER_BELOW)
    print("Got untouch")
  end

  print("Pads:", sjson.encode(pads))

end
```

## touchObj:intrEnable()

Enable interrupt on the touch sensor hardware. You can specify `intrInitAtStart=false` during `touch.create()` and thus you would want to call this method later on after configuring your pad thresholds.

#### Syntax
`tp:intrEnable()`

#### Parameters
None

#### Returns
`nil`

#### Example
```lua
tp:intrEnable() -- Enable interrupt
```

## touchObj:intrDisable()

Disable interrupt on the touch sensor hardware.

#### Syntax
`tp:intrDisable()`

#### Parameters
None

#### Returns
`nil`

#### Example
```lua
tp:intrDisable() -- Disable interrupt
```
