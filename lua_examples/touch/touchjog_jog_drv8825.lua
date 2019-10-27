-- DRV8825 driver
-- Provides gpio init, enable, disable, direction, and pulse counting for steps
-- The DVR8825 runs over normal GPIO

local m = {}

m.pinStep = 4 
m.pinDir = 0 -- it is 0 cuz that is bootstrap pin which inconsequential on direction pin
m.pinSleep = 16 

-- Loopback pulse in from m.pinStep
-- You need to connect a physical wire from m.pinStep 4 to pin 36 for step counting to work
m.pinPulseIn = 36 -- 36 (sens_vp)

-- Microsteps pins in case you want to manage microsteps from gpio
m.pinM0 = 17
m.pinM1 = 18
m.pinM2 = 19

-- Min/max steps allowed 32768 + or - is allowed
m.stepMax = 32000
m.stepMin = -32000

m.isDebug = false

-- Pulse counter object
m.pcnt = nil

m._isInitted = false
m._onLimit = nil

-- Pass in a table of settings
-- @param tbl.initStepDirEnPins  Defaults to false
-- @param tbl.pinStep  Defaults to 4
-- @param tbl.pinDir   Defaults to 0
-- @param tbl.pinEn    Defaults to 16
-- @param tbl.isDebug  Defaults to false. Turn on for extra logging.
-- @param tbl.onLimit Callback when stepMax or stepMin is hit
-- @param tbl.stepLimitMax Defaults to 32000
-- @param tbl.stepLimitMin Defaults to -32000
-- Example motor.init({initStepDirEnPins=true, })
function m.init(tbl)
  
  if m._isInitted then 
    print("DRV8825 already initted")
    return 
  end
  
  m._isInitted = true
  
  if tbl.pinStep ~= nil then m.pinStep = tbl.pinStep end
  if tbl.pinDir ~= nil then m.pinDir = tbl.pinDir end
  if tbl.pinEn ~= nil then m.pinSleep = tbl.pinEn end
  if tbl.isDebug == true then m.isDebug = true end
  if tbl.onLimit ~= nil then m._onLimit = tbl.onLimit end
  if tbl.stepLimitMax ~= nil then m.stepMax = tbl.stepLimitMax end
  if tbl.stepLimitMin ~= nil then m.stepMin = tbl.stepLimitMin end
  
  -- defaults to false
  if tbl.initStepDirEnPins == true then
    
    
    gpio.config({
      gpio= {
        m.pinStep, m.pinSleep, m.pinDir,
        m.pinM0, m.pinM1, m.pinM2
      },
      dir=gpio.IN_OUT,
    })
  
    gpio.write(m.pinStep, 0)
    
    m.disable()
    
    m.dirFwd()
    
    -- for full steps, all low
    -- for max micro-stepping, all high
    gpio.write(m.pinM0, 0)
    gpio.write(m.pinM1, 0)
    gpio.write(m.pinM2, 0)

  end
  
  -- Start counting pulses on the loopback signal
  m.initPulseCtr()
  print("initted pulse ctr")
  
end

function m.initPulseCtr()
  
  -- Setup the pulse counter to watch the steps
  -- Adhere to the direction pin as well to know automatically fwd/rev
  -- so our steps are accurate to where the motor is
  m.pcnt = pulsecnt.create(0, m.onPulseCnt, false)
  m.pcnt:chan0Config(
    m.pinPulseIn, -- 36 (sens_vp), 39 (sens_vn), m.pinStep, m.pinPulseIn, --pinPulseIn --pulse_gpio_num
    m.pinDir, --ctrl_gpio_num If no control is desired specify PCNT_PIN_NOT_USED 
    pulsecnt.PCNT_COUNT_INC, --pos_mode PCNT positive edge count mode
    pulsecnt.PCNT_COUNT_DIS, --neg_mode PCNT negative edge count mode
    pulsecnt.PCNT_MODE_REVERSE, --lctrl_mode PCNT_MODE_KEEP, PCNT_MODE_REVERSE, PCNT_MODE_DISABLE
    pulsecnt.PCNT_MODE_KEEP, --hctrl_mode PCNT_MODE_KEEP, PCNT_MODE_REVERSE, PCNT_MODE_DISABLE
    -32768, --counter_l_lim
    32767  --counter_h_lim
  )
  m.pcnt:setThres(m.stepMin, m.stepMax)
  m.pcnt:clear()
  
end

function m.onPulseCnt(unit, isThr0, isThr1, isLLim, isHLim, isZero)
  
  print("Got pulse counter.")
  print("unit:", unit, "isThr0:", isThr0, "isThr1:", isThr1)
  print("isLLim:", isLLim, "isHLim:", isHLim, "isZero:", isZero)
  
  if isThr0 or isThr1 then
    m.disable()
    
    -- if callback from user, then call it
    if m.onLimit ~= nil then
      m.onLimit(isThr0, isThr1)
    end
    
    -- m.pause()
    -- m.stop()
    if isThr0 then
      print("Hit endstop in negative direction")
    else
      print("Hit endstop in positive direction")
    end
  end
  
end


m.DIR_FWD = 1
m.DIR_REV = 0
m._dir = nil
function m.setDir(dir)
  if dir == m.DIR_FWD then
    if m._dir == m.DIR_FWD then
      -- already set. ignore.
      if m.isDebug then print("Dir fwd already set. Ignoring.") end
      return
    end
    gpio.write(m.pinDir,1)
    m._dir = m.DIR_FWD
    if m.isDebug then print("Set dir fwd") end
  else
    if m._dir == m.DIR_REV then
      -- already set. ignore.
      if m.isDebug then print("Dir rev already set. Ignoring.") end
      return
    end
    gpio.write(m.pinDir,0)
    m._dir = m.DIR_REV
    if m.isDebug then print("Set dir rev") end
  end
end

function m.dirFwd()
  m.setDir(m.DIR_FWD)
end

function m.dirRev()
  m.setDir(m.DIR_REV)
end

function m.dirToggle()
  if m._dir == m.DIR_FWD then
    m.dirRev()
  else
    m.dirFwd()
  end
end

function m.disable()
  -- drv8825 low makes sleep / high make active
  gpio.write(m.pinSleep, 0)
  if m.isDebug then print("Sleeping motor (disable)") end
end 

function m.enable()
  -- drv8825 low makes sleep / high make active
  gpio.write(m.pinSleep, 1)
  if m.isDebug then print("Waking motor (enable)") end
end

return m