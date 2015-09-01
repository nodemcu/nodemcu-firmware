---------------------------------------------------------------
-- MPR121 I2C module for NodeMCU
-- Based on code from: http://bildr.org/2011/05/mpr121_arduino/
-- Tiago Custódio <tiagocustodio1@gmail.com>
---------------------------------------------------------------

local moduleName = ...
local M = {}
_G[moduleName] = M

-- Default value for i2c communication
local id = 0

--device address
local devAddr = 0

--make it faster
local i2c = i2c

function M.setRegister(address, r, v)
	i2c.start(id)
	local ans = i2c.address(id, address, i2c.TRANSMITTER)
	i2c.write(id, r)
	i2c.write(id, v)
	i2c.stop(id)
end

function M.init(address)
	devAddr = address
	M.setRegister(devAddr, 0x5E, 0x00) -- ELE_CFG

	-- Section A - Controls filtering when data is > baseline.
	M.setRegister(devAddr, 0x2B, 0x01) -- MHD_R
	M.setRegister(devAddr, 0x2C, 0x01) -- NHD_R
	M.setRegister(devAddr, 0x2D, 0x00) -- NCL_R
	M.setRegister(devAddr, 0x2E, 0x00) -- FDL_R

	-- Section B - Controls filtering when data is < baseline.
	M.setRegister(devAddr, 0x2F, 0x01) -- MHD_F
	M.setRegister(devAddr, 0x30, 0x01) -- NHD_F
	M.setRegister(devAddr, 0x31, 0xFF) -- NCL_F
	M.setRegister(devAddr, 0x32, 0x02) -- FDL_F

	-- Section C - Sets touch and release thresholds for each electrode
	for i = 0, 11 do
		M.setRegister(devAddr, 0x41+(i*2), 0x06) -- ELE0_T
		M.setRegister(devAddr, 0x42+(i*2), 0x0A) -- ELE0_R
	end

	-- Section D - Set the Filter Configuration - Set ESI2
	M.setRegister(devAddr, 0x5D, 0x04) -- FIL_CFG

	-- Section E - Electrode Configuration - Set 0x5E to 0x00 to return to standby mode
	M.setRegister(devAddr, 0x5E, 0x0C) -- ELE_CFG

    tmr.delay(50000) -- Delay to let the electrodes settle after config
end

function M.readTouchInputs()
	i2c.start(id)
	i2c.address(id, devAddr, i2c.RECEIVER)
	local c = i2c.read(id, 2)
	i2c.stop(id)
	local LSB = tonumber(string.byte(c, 1))
	local MSB = tonumber(string.byte(c, 2))
	local touched = bit.lshift(MSB, 8) + LSB
	local t = {}
	for i = 0, 11 do
		t[i+1] = bit.isset(touched, i) and 1 or 0
	end
	return t
end

return M
