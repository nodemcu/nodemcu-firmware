-- ***************************************************************************
-- tsl2561.lua
-- Module for ESP8266 with nodeMCU
-- Ported from github.com/Seeed-Studio/Grove_Digital_Light_Sensor
--
-- Copyright (c) 2012 seeed technology inc.
-- Website    : www.seeed.cc
-- Author     : zhangkun
-- Create Time:
-- Change Log : 2015-07-21: Ported original code to Lua
--              by Marius Schmeding (skybus.io)
--
-- MIT License, http://opensource.org/licenses/MIT
-- ***************************************************************************

local TSL2561_Control = 0x80
local TSL2561_Timing = 0x81
local TSL2561_Interrupt = 0x86
local TSL2561_Channel0L = 0x8C
local TSL2561_Channel0H = 0x8D
local TSL2561_Channel1L = 0x8E
local TSL2561_Channel1H = 0x8F

local TSL2561_Address = 0x29   -- device address

local LUX_SCALE     = 14       -- scale by 2^14
local RATIO_SCALE   = 9        -- scale ratio by 2^9
local CH_SCALE      = 10       -- scale channel values by 2^10
local CHSCALE_TINT0 = 0x7517   -- 322/11 * 2^CH_SCALE
local CHSCALE_TINT1 = 0x0fe7   -- 322/81 * 2^CH_SCALE

-- Scale table
local S = {}
S.K1T = 0x0040   -- 0.125 * 2^RATIO_SCALE
S.B1T = 0x01f2   -- 0.0304 * 2^LUX_SCALE
S.M1T = 0x01be   -- 0.0272 * 2^LUX_SCALE
S.K2T = 0x0080   -- 0.250 * 2^RATIO_SCA
S.B2T = 0x0214   -- 0.0325 * 2^LUX_SCALE
S.M2T = 0x02d1   -- 0.0440 * 2^LUX_SCALE
S.K3T = 0x00c0   -- 0.375 * 2^RATIO_SCALE
S.B3T = 0x023f   -- 0.0351 * 2^LUX_SCALE
S.M3T = 0x037b   -- 0.0544 * 2^LUX_SCALE
S.K4T = 0x0100   -- 0.50 * 2^RATIO_SCALE
S.B4T = 0x0270   -- 0.0381 * 2^LUX_SCALE
S.M4T = 0x03fe   -- 0.0624 * 2^LUX_SCALE
S.K5T = 0x0138   -- 0.61 * 2^RATIO_SCALE
S.B5T = 0x016f   -- 0.0224 * 2^LUX_SCALE
S.M5T = 0x01fc   -- 0.0310 * 2^LUX_SCALE
S.K6T = 0x019a   -- 0.80 * 2^RATIO_SCALE
S.B6T = 0x00d2   -- 0.0128 * 2^LUX_SCALE
S.M6T = 0x00fb   -- 0.0153 * 2^LUX_SCALE
S.K7T = 0x029a   -- 1.3 * 2^RATIO_SCALE
S.B7T = 0x0018   -- 0.00146 * 2^LUX_SCALE
S.M7T = 0x0012   -- 0.00112 * 2^LUX_SCALE
S.K8T = 0x029a   -- 1.3 * 2^RATIO_SCALE
S.B8T = 0x0000   -- 0.000 * 2^LUX_SCALE
S.M8T = 0x0000   -- 0.000 * 2^LUX_SCALE

S.K1C = 0x0043   -- 0.130 * 2^RATIO_SCALE
S.B1C = 0x0204   -- 0.0315 * 2^LUX_SCALE
S.M1C = 0x01ad   -- 0.0262 * 2^LUX_SCALE
S.K2C = 0x0085   -- 0.260 * 2^RATIO_SCALE
S.B2C = 0x0228   -- 0.0337 * 2^LUX_SCALE
S.M2C = 0x02c1   -- 0.0430 * 2^LUX_SCALE
S.K3C = 0x00c8   -- 0.390 * 2^RATIO_SCALE
S.B3C = 0x0253   -- 0.0363 * 2^LUX_SCALE
S.M3C = 0x0363   -- 0.0529 * 2^LUX_SCALE
S.K4C = 0x010a   -- 0.520 * 2^RATIO_SCALE
S.B4C = 0x0282   -- 0.0392 * 2^LUX_SCALE
S.M4C = 0x03df   -- 0.0605 * 2^LUX_SCALE
S.K5C = 0x014d   -- 0.65 * 2^RATIO_SCALE
S.B5C = 0x0177   -- 0.0229 * 2^LUX_SCALE
S.M5C = 0x01dd   -- 0.0291 * 2^LUX_SCALE
S.K6C = 0x019a   -- 0.80 * 2^RATIO_SCALE
S.B6C = 0x0101   -- 0.0157 * 2^LUX_SCALE
S.M6C = 0x0127   -- 0.0180 * 2^LUX_SCALE
S.K7C = 0x029a   -- 1.3 * 2^RATIO_SCALE
S.B7C = 0x0037   -- 0.00338 * 2^LUX_SCALE
S.M7C = 0x002b   -- 0.00260 * 2^LUX_SCALE
S.K8C = 0x029a   -- 1.3 * 2^RATIO_SCALE
S.B8C = 0x0000   -- 0.000 * 2^LUX_SCALE
S.M8C = 0x0000   -- 0.000 * 2^LUX_SCALE

local moduleName = ... 
local M = {}
_G[moduleName] = M

-- i2c interface ID
local id = 0

-- local vars
local ch0,ch1,chScale,channel1,channel0,ratio1,b,m,temp,lux = 0

-- Wrapping I2C functions to retain original calls
local Wire = {}
function Wire.beginTransmission(ADDR)
    i2c.start(id)
    i2c.address(id, ADDR, i2c.TRANSMITTER)
end

function Wire.write(commands)
    i2c.write(id, commands)
end

function Wire.endTransmission()
    i2c.stop(id)
end

function Wire.requestFrom(ADDR, length)
    i2c.start(id)
    i2c.address(id, ADDR,i2c.RECEIVER)
    c = i2c.read(id, length)
    i2c.stop(id)
    return string.byte(c)
end

local function readRegister(deviceAddress, address)
     Wire.beginTransmission(deviceAddress)
     Wire.write(address)                -- register to read
     Wire.endTransmission()
     value = Wire.requestFrom(deviceAddress, 1) -- read a byte
     return value
end

local function writeRegister(deviceAddress, address, val)
     Wire.beginTransmission(deviceAddress)  -- start transmission to device
     Wire.write(address)                    -- send register address
     Wire.write(val)                        -- send value to write
     Wire.endTransmission()                 -- end transmission
end

function M.getLux()
    local CH0_LOW=readRegister(TSL2561_Address,TSL2561_Channel0L)
    local CH0_HIGH=readRegister(TSL2561_Address,TSL2561_Channel0H)
    --read two bytes from registers 0x0E and 0x0F
    local CH1_LOW=readRegister(TSL2561_Address,TSL2561_Channel1L)
    local CH1_HIGH=readRegister(TSL2561_Address,TSL2561_Channel1H)

    ch0 = bit.bor(bit.lshift(CH0_HIGH,8),CH0_LOW)
    ch1 = bit.bor(bit.lshift(CH1_HIGH,8),CH1_LOW)
end

function M.init(sda, scl)
   i2c.setup(id, sda, scl, i2c.SLOW)
   writeRegister(TSL2561_Address,TSL2561_Control,0x03)  -- POWER UP
   writeRegister(TSL2561_Address,TSL2561_Timing,0x00)  --No High Gain (1x), integration time of 13ms
   writeRegister(TSL2561_Address,TSL2561_Interrupt,0x00)
   writeRegister(TSL2561_Address,TSL2561_Control,0x00)  -- POWER Down
end

function M.readVisibleLux()
   writeRegister(TSL2561_Address,TSL2561_Control,0x03)  -- POWER UP
   tmr.delay(14000)
   M.getLux()

   writeRegister(TSL2561_Address,TSL2561_Control,0x00)  -- POWER Down
   if(ch0/ch1 < 2 and ch0 > 4900) then
     return -1  -- ch0 out of range, but ch1 not. the lux is not valid in this situation.
   end
   return M.calculateLux(0, 0, 0)  -- T package, no gain, 13ms
end

function M.calculateLux(iGain, tInt, iType)
   if tInt == 0 then -- 13.7 msec
      chScale = CHSCALE_TINT0
   elseif tInt == 1 then -- 101 msec
      chScale = CHSCALE_TINT1
   else -- assume no scaling
      chScale = bit.lshift(1,CH_SCALE)
   end

   if (not iGain) then chScale = bit.lshift(chScale,4) end -- scale 1X to 16X
   -- scale the channel values
   channel0 = bit.rshift((ch0 * chScale),CH_SCALE)
   channel1 = bit.rshift((ch1 * chScale),CH_SCALE)

   ratio1 = 0
   if channel0 ~= 0 then ratio1 = bit.lshift(channel1,(RATIO_SCALE+1))/channel0 end
   -- round the ratio value
   ratio = bit.rshift((ratio1 + 1),1)

   if iType == 0 then -- T package
       if ratio >= 0 and ratio <= S.K1T then
          b=S.B1T
          m=S.M1T
       elseif ratio <= S.K2T then
          b=S.B2T 
          m=S.M2T
       elseif ratio <= S.K3T then
          b=S.B3T 
          m=S.M3T
       elseif ratio <= S.K4T then
          b=S.B4T
          m=S.M4T
       elseif ratio <= S.K5T then
          b=S.B5T
          m=S.M5T
       elseif ratio <= S.K6T then
          b=S.B6T
          m=S.M6T
       elseif ratio <= S.K7T then
          b=S.B7T
          m=S.M7T
       elseif ratio > S.K8T then
          b=S.B8T
          m=S.M8T
       end
   elseif iType == 1 then -- CS package
       if ratio >= 0 and ratio <= S.K1C then
          b=S.B1C
          m=S.M1C
       elseif ratio <= S.K2C then
          b=S.B2C
          m=S.M2C
       elseif ratio <= S.K3C then
          b=S.B3C
          m=S.M3C
       elseif ratio <= S.K4C then
          b=S.B4C
          m=S.M4C
       elseif ratio <= S.K5C then
          b=S.B5C
          m=S.M5C
       elseif ratio <= S.K6C then
          b=S.B6C
          m=S.M6C
       elseif ratio <= S.K7C then
          b=S.B7C
          m=S.M7C
       end
   end
   temp=((channel0*b)-(channel1*m))
   if temp<0 then temp=0 end
   temp = temp + bit.lshift(1,(LUX_SCALE-1))
   -- strip off fractional portion
   lux = bit.rshift(temp,LUX_SCALE)
   return lux
end

return M
