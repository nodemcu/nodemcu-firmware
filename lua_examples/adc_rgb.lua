--
-- Light sensor on adc0(A0), RGB LED connected to gpio12(D6) Green, gpio13(D7) Blue & gpio15(D8) Red.
-- This works out of the box on the typical ESP8266 evaluation boards with Battery Holder
--
-- It uses the input from the sensor to drive a "rainbow" effect on the RGB LED
-- Includes a very "pseudoSin" function
--
-- Required C Modules: adc, tmr, pwm

local redLed, greenLed, blueLed = 8, 6, 7

local function setRGB(r,g,b)
  pwm.setduty(redLed, r)
  pwm.setduty(greenLed, g)
  pwm.setduty(blueLed, b)
end

-- this is perhaps the lightest weight sin function in existence
-- Given an integer from 0..128, 0..512 approximating 256 + 256 * sin(idx*Pi/256)
-- This is first order square approximation of sin, it's accurate around 0 and any multiple of 128 (Pi/2),
-- 92% accurate at 64 (Pi/4).
local function pseudoSin(idx)
  idx = idx % 128
  local lookUp = 32 - idx % 64
  local val = 256 - (lookUp * lookUp) / 4
  if (idx > 64) then
      val = - val;
  end
  return 256+val
end

do
  pwm.setup(redLed, 500, 512)
  pwm.setup(greenLed,500, 512)
  pwm.setup(blueLed, 500, 512)
  pwm.start(redLed)
  pwm.start(greenLed)
  pwm.start(blueLed)

  tmr.create():alarm(20, tmr.ALARM_AUTO, function()
    local idx = 3 * adc.read(0) / 2
    local r = pseudoSin(idx)
    local g = pseudoSin(idx + 43) -- ~1/3rd of 128
    local b = pseudoSin(idx + 85) -- ~2/3rd of 128
    setRGB(r,g,b)
  end)
end
