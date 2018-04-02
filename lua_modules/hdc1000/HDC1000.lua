-------------------------------------------------------

-- This library was written for the Texas Instruments 
-- HDC1000 temperature and humidity sensor. 
-- It should work for the HDC1008 too.
-- Written by Francesco Truzzi (francesco@truzzi.me)
-- Released under GNU GPL v2.0 license.

-------------------------------------------------------

-------------- NON-DEFAULT CONFIG VALUES --------------
------------- config() optional arguments -------------

--	HDC1000_HEAT_OFF			0x00	(heater)
--	HDC1000_TEMP_11BIT			0x40	(resolution)
-- 	HDC1000_HUMI_11BIT			0x01 	(resolution)
--	HDC1000_HUMI_8BIT			0x20 	(resolution)

-------------------------------------------------------

local modname = ...
local M = {}
_G[modname] = M

local id = 0
local i2c = i2c
local delay = 20000
local _drdyn_pin

local HDC1000_ADDR = 0x40

local HDC1000_TEMP = 0x00
local HDC1000_HUMI = 0x01
local HDC1000_CONFIG = 0x02

local HDC1000_HEAT_ON = 0x20
local HDC1000_TEMP_HUMI_14BIT = 0x00

-- reads 16bits from the sensor
local function read16()
	i2c.start(id)
	i2c.address(id, HDC1000_ADDR, i2c.RECEIVER)
	data_temp = i2c.read(0, 2)
	i2c.stop(id)
	data = bit.lshift(string.byte(data_temp, 1, 1), 8) + string.byte(data_temp, 2, 2)
	return data
end

-- sets the register to read next
local function setReadRegister(register)
	i2c.start(id)
	i2c.address(id, HDC1000_ADDR, i2c.TRANSMITTER)
	i2c.write(id, register)
	i2c.stop(id)
end

-- writes the 2 configuration bytes
local function writeConfig(config)
	i2c.start(id)
	i2c.address(id, HDC1000_ADDR, i2c.TRANSMITTER)
	i2c.write(id, HDC1000_CONFIG, config, 0x00)
	i2c.stop(id)
end

-- returns true if battery voltage is < 2.7V, false otherwise
function M.batteryDead()
	setReadRegister(HDC1000_CONFIG)
	return(bit.isset(read16(), 11))

end

-- setup i2c
function M.setup(drdyn_pin)
	_drdyn_pin = drdyn_pin
end

function M.config(addr, resolution, heater)
	-- default values are set if the function is called with no arguments
	HDC1000_ADDR = addr or HDC1000_ADDR
	resolution = resolution or HDC1000_TEMP_HUMI_14BIT
	heater = heater or HDC1000_HEAT_ON
	writeConfig(bit.bor(resolution, heater))
end

-- outputs temperature in Celsius degrees
function M.getHumi()
	setReadRegister(HDC1000_HUMI)
	if(_drdyn_pin ~= false) then
		gpio.mode(_drdyn_pin, gpio.INPUT)
		while(gpio.read(_drdyn_pin)==1) do
	end
	else tmr.delay(delay) end
	return(read16()/65535.0*100)
end

-- outputs humidity in %RH
function M.getTemp()
	setReadRegister(HDC1000_TEMP)
	if(_drdyn_pin ~= false) then
		gpio.mode(_drdyn_pin, gpio.INPUT)
		while(gpio.read(_drdyn_pin)==1) do
	end
	else tmr.delay(delay) end
	return(read16()/65535.0*165-40)
end

return M

