--------------------------------------------------------------------------------
-- Wiegand module for NODEMCU
-- LICENCE: http://opensource.org/licenses/MIT
-- Birkir Gudjonsson <birkir.gudjonsson@gmail.com>
--------------------------------------------------------------------------------

local moduleName = ...
local M = {}
_G[moduleName] = M

local reader
local count

function high ()
	low()
	reader = reader | 1
end

function low ()
	count = count + 1
	reader = reader << 1
end

function init(a, b)
	tmr.delay(10)
	-- reset pins
	gpio.mode(a, gpio.OUTPUT)
	gpio.write(a, gpio.HIGH)
	gpio.write(a, gpio.LOW)
	gpio.mode(a, gpio.INPUT)
	gpio.write(a, gpio.HIGH)
	gpio.mode(b, gpio.OUTPUT)
	gpio.write(b, gpio.HIGH)
	gpio.write(b, gpio.LOW)
	gpio.mode(b, gpio.INPUT)
	gpio.write(b, gpio.HIGH)
	gpio.delay(10);
end

local function setup (pin1, pin2)
	gpio.mode(pin1, gpio.INT)
	gpio.mode(pin2, gpio.INT)
	init(pin1, pin2)
	gpio.trig(pin1, "down", high)
	gpio.trig(pin2, "down", low)
end

local function read ()
	if count >= 32 then
		siteCode = (reader >> 17) & 0x3ff;
		serialNumber = (reader >> 1) & 0x3fff;
		reader = 0
		count = 0
		return {site=siteCode, serial=serialNumber}
	end
	return false
end
