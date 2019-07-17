-- ChiliPeppr Joystick controller
-- Used to control analog joysticks that are implemented just using
-- variable resistors where you feed in 3.3v and get out a variable voltage
-- based on position of the joystick. Joysticks like the Playstation portable
-- or Xbox joysticks work this way and are readily available on Amazon, Ebay,
-- or Aliexpress.
--
-- Working example of using this library:
-- https://www.youtube.com/watch?v=ITgh5epyPRk
--
-- Visit http://chilipeppr.com/esp32
-- By John Lauer
--
-- There is no license needed to use/modify this software. It is given freely
-- to the open source community. Modify at will.
-- 
-- To use:
-- joystick = require("joystick")
-- joystick.on("xy", function(x, y)
--   print("Got xy change. x:", x, "y:", y)
-- end)
-- joystick.on("x", function(x)
--   print("Got x change. x:", x)
-- end)
-- joystick.on("y", function(y)
--   print("Got y change. y:", y)
-- end)
-- joystick.on("xcenter", function()
--   print("Got x center")
-- end)
-- joystick.on("ycenter", function()
--   print("Got y center")
-- end)
-- joystick.on("button", function(val)
--   print("Got button:", val)
-- end)
-- joystick.init()

-- For test/debug
-- if m.tmr then m.tmr:stop(); m.tmr:unregister() end 

local m = {}
-- m = {}

m.pinAdcX = 6 -- ADC 6: GPIO34
-- m.pinAdcY = 3 -- ADC 3: GPIO39
-- m.pinAdcY = 7 -- ADC 7: GPIO35
m.pinAdcY = 5 -- ADC 5: GPIO33
-- m.pinSwitch = 18
m.pinSwitch = 32

-- For TTGO
-- m.centerX = 1850
-- m.centerXMin = 1800
-- m.centerXMax = 1915
-- m.centerY = 1850
-- m.centerYMin = 1800
-- m.centerYMax = 1915

-- For Wemos battery powered
m.centerX = 1800
m.centerXMin = 1700
m.centerXMax = 1870
m.centerY = 1800
m.centerYMin = 1700
m.centerYMax = 1870


m.isInitted = false
m.tmr = tmr.create()

---
-- @name init
-- @description Call this first to setup your ADC ports and button port. You
-- need to connect X to ADC 6: GPIO34, Y to ADC5: GPIO33, and the button to GPIO32.
-- If you need to use alternate ports you should change the code in joystick.lua.
-- Also, 11db attenuation is turned on by default because it is the best
-- performance for analog joysticks. If you are finding a broader range of voltage
-- is coming back from your joystick, adjust the attenuation in the init() method.
-- @returns nil
function m.init()
  
  if m.isInitted then
    return
  end
  
  print("Initting joystick pins. X: " .. m.pinAdcX .. ", Y: " .. m.pinAdcY .. ", Switch: " .. m.pinSwitch)
  
  -- Initialise the pins
  gpio.config( { gpio=m.pinSwitch, dir=gpio.IN, pull=gpio.PULL_UP } )
  m.startSwitchInterrupt()
  
  -- adc.setwidth(adc.ADC1, 9)  -- read 9 bit on ADC
  adc.setwidth(adc.ADC1, 12)  -- read 12 bit on ADC
  adc.setup(adc.ADC1, m.pinAdcX, adc.ATTEN_11db    )
  adc.setup(adc.ADC1, m.pinAdcY, adc.ATTEN_11db    )
  
  m.tmr:alarm(250, tmr.ALARM_AUTO, function()
    m.readXY()
    -- m.readY()
    -- m.readX()
  end)
  
  m.isInitted = true
  
end

m.onX = nil
m.onXCenter = nil
m.onY = nil
m.onYCenter = nil
m.onXY = nil
m.onButton = nil 
---
-- @name on
-- @description Attach to the callback events available from this joystick library
-- @param event_name A string of the event you want a callback on. "x", "y", "xy", "xcenter", "ycenter", "button"
-- @param func The callback to receive after the event
-- @returns nil
function m.on(event_name, func)
  if event_name == "x" then 
    m.onX = func 
  elseif event_name == "xcenter" then
    m.onXCenter = func 
  elseif event_name == "y" then 
    m.onY = func 
  elseif event_name == "ycenter" then
    m.onYCenter = func
  elseif event_name == "xy" then
    m.onXY = func
  elseif event_name == "button" then 
    m.onButton = func 
  end 
end  

m.lastX = 0
function m.readX()
  local val = adc.read(adc.ADC1, m.pinAdcX)
  if m.lastX == val then return end
  m.lastX = val
  print("X: " .. val)
  if m.onX then m.onX(val) end 
end

m.lastY = 0
function m.readY()
  local val = adc.read(adc.ADC1, m.pinAdcY)
  if m.lastY == val then return end
  m.lastY = val
  print("Y: " .. val)
  if m.onY then m.onY(val) end 
end

m.lastXWasCenter = true
m.lastYWasCenter = true
function m.readXY()
  local valX = adc.read(adc.ADC1, m.pinAdcX)
  local valY = adc.read(adc.ADC1, m.pinAdcY)
  
  -- if x or y is new value then send xy update
  local isXYUpdated = false
  
  -- do some level setting for center point
  if valX >= m.centerXMin and valX <= m.centerXMax then 
    -- consider it a center point
    -- valX = m.centerX
    
    -- see if last X was center, and if so ignore 
    if m.lastXWasCenter then
      -- do nothing
    else 
    
      -- last x was not center, so fire off event
      m.lastXWasCenter = true
      if m.onXCenter then 
        node.task.post(node.task.LOW_PRIORITY, function()
          m.onXCenter(m.centerX) 
        end)
      end
    end 
    
  else 
    -- x is not on center, so fire event
    m.lastXWasCenter = false
    isXYUpdated = true
    if m.onX then 
      node.task.post(node.task.LOW_PRIORITY, function()
        m.onX(valX) 
      end)
    end
  end 
  
  if valY >= m.centerYMin and valY <= m.centerYMax then 
    -- consider it a center point
    -- valY = m.centerY
    -- see if last Y was center, and if so ignore 
    if m.lastYWasCenter then
      -- do nothing
    else
    
      -- last x was not center, so fire off event
      m.lastYWasCenter = true
      if m.onYCenter then 
        node.task.post(node.task.LOW_PRIORITY, function()
          m.onYCenter(m.centerY) 
        end)
      end
    end 
    
  else 
    -- y is not on center, so fire event
    m.lastYWasCenter = false
    isXYUpdated = true
    if m.onY then 
      node.task.post(node.task.LOW_PRIORITY, function()
        m.onY(valY) 
      end)
    end
  end
  
  if isXYUpdated then
    if m.onXY then 
      node.task.post(node.task.LOW_PRIORITY, function()
        m.onXY(valX, valY) 
      end)
    end
  end
  
  -- print("X: " .. valX .. ", Y: " .. valY)
  
end

function m.readSwitch()
  local val = gpio.read(m.pinSwitch)
  print("Switch: ", val)
  if m.onButton then m.onButton(val) end
end

function m.startSwitchInterrupt()
  gpio.trig(m.pinSwitch, gpio.INTR_UP_DOWN, m.onSwitchInterrupt)
  print("Set interrupt for switch.")
end 

m.lastSwitchVal = 0
function m.onSwitchInterrupt(pin, level)
  if level == m.lastSwitchVal then return end 
  m.lastSwitchVal = level
  print("Got switch interrupt. pin: ", pin, ", level: ", level)
end

-- m.init()

return m 


                                