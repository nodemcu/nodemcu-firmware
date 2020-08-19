--[[
	This Lua module provides access to the MCP23017 module.

    The MCP23017 is a port expander and provides 16 channels for inputs and outputs.
    Up to 8 devices (128 channels) are possible by the configurable address (A0 - A2 Pins).

    Due to the 16 channels, 2 bytes are required for switching outputs or reading input signals. These are A and B.
    A single pin can be set or a whole byte.

    The numbering of the individual pins starts at 0 and ends with 7.
    The numbers are for each register GPIO A and GPIO B.

	The module requires `i2c` and `bit` C module built into firmware.

	@author Marcel P. | Plomi.net
	@github https://github.com/plomi-net
	@version 1.0.0
]]

local i2c, string = i2c, string
local issetBit = bit.isset
local setBit = bit.set
local clearBit = bit.clear

-- metatable
local mcp23017 = {}
mcp23017.__index = mcp23017

-- registers (not used registers are commented out)
local MCP23017_IODIRA = 0x00
local MCP23017_IODIRB = 0x01
local MCP23017_DEFVALA = 0x06
local MCP23017_DEFVALB = 0x07
local MCP23017_GPIOA = 0x12
local MCP23017_GPIOB = 0x13
--[[
local MCP23017_IPOLA = 0x02
local MCP23017_IPOLB = 0x03
local MCP23017_GPINTENA = 0x04
local MCP23017_GPINTENB = 0x05
local MCP23017_DEFVALA = 0x06
local MCP23017_DEFVALB = 0x07
local MCP23017_INTCONA = 0x08
local MCP23017_INTCONB = 0x09
local MCP23017_IOCON = 0x0A
local MCP23017_IOCON2 = 0x0B
local MCP23017_GPPUA = 0x0C
local MCP23017_GPPUB = 0x0D
local MCP23017_INTFA = 0x0E
local MCP23017_INTFB = 0x0F
local MCP23017_INTCAPA = 0x10
local MCP23017_INTCAPB = 0x11
local MCP23017_OLATA = 0x14
local MCP23017_OLATB = 0x15
]]

-- check device is available on address
local function checkDevice(self)
    i2c.start(self.i2cId)
    local response = i2c.address(self.i2cId, self.address, i2c.TRANSMITTER)
    i2c.stop(self.i2cId)
    if response ~= true then
        print("MCP23017 device on " .. string.format('0x%02X', self.address) .. " not found")
    end
    return response
end

-- check device address (0x20 to 0x27)
local function checkAddress(self)
    if (self.address > 31 and self.address < 40) then
        return true
    else
        print("MCP23017 address is out of range")
        return false
    end
end

-- write byte
local function writeByte(self, registerAddr, val)
    i2c.start(self.i2cId)
    i2c.address(self.i2cId, self.address, i2c.TRANSMITTER)
    i2c.write(self.i2cId, registerAddr)
    i2c.write(self.i2cId, val)
    i2c.stop(self.i2cId)
end

-- read byte
local function readByte(self, registerAddr)
    i2c.start(self.i2cId)
    i2c.address(self.i2cId, self.address, i2c.TRANSMITTER)
    i2c.write(self.i2cId, registerAddr)
    i2c.stop(self.i2cId)
    i2c.start(self.i2cId)
    i2c.address(self.i2cId, self.address, i2c.RECEIVER)
    local data = i2c.read(self.i2cId, 1)
    i2c.stop(self.i2cId)
    return string.byte(data)
end

-- get IO dir register
local function getDirRegisterAddr(self, bReg)
    if bReg == self.GPB then
        return MCP23017_IODIRB
    else
        return MCP23017_IODIRA
    end
end

-- get GPIO register address
local function getGPIORegisterAddr(self, bReg)
    if bReg == self.GPB then
        return MCP23017_GPIOB
    else
        return MCP23017_GPIOA
    end
end

-- check pin is in range
local function checkPinIsInRange(pin)
    if pin > 7 or pin < 0 then
        print("The pin must be between 0 and 7")
        return nil
    end
    return pin
end

-- setup internal pullup
function mcp23017:setInternalPullUp(bReg, iByte)
    if bReg == self.GPB then
        writeByte(self, MCP23017_DEFVALB, iByte)
    else
        writeByte(self, MCP23017_DEFVALA, iByte)
    end
end

-- set default GPIO mode
local function setDefaultMode(self, bReg, iByte)
    writeByte(self, getDirRegisterAddr(self, bReg), iByte)
end

-- reset gpio mode
function mcp23017:reset()
    -- all to input
    setDefaultMode(self, self.GPA, 0xFF)
    setDefaultMode(self, self.GPB, 0xFF)
end

-- setup device
function mcp23017:setup(address, i2cId)

    self.address = address
    self.i2cId = i2cId
    if (checkAddress(self) ~= true) or (checkDevice(self) ~= true) then
        self.deviceOk = false
        return 0
    else
        self.deviceOk = true
        self:reset()
        return 1
    end
end

-- set mode for a pin
function mcp23017:setMode(bReg, pin, mode)
    if self.deviceOk == false then
        return nil
    end

    local inReq = getDirRegisterAddr(self, bReg)
    local inPin = checkPinIsInRange(pin)
    local response = readByte(self, inReq)
    local newState

    if mode == self.OUTPUT then
        newState = clearBit(response, inPin)
    else
        newState = setBit(response, inPin)
    end

    writeByte(self, inReq, newState)
    return true
end

-- set pin to low or high
function mcp23017:setPin(bReg, pin, state)
    if self.deviceOk == false then
        return nil
    end

    local inReq = getGPIORegisterAddr(self, bReg)
    local inPin = checkPinIsInRange(pin)
    local response = readByte(self, inReq)
    local newState

    if state == self.HIGH then
        newState = setBit(response, inPin)
    else
        newState = clearBit(response, inPin)
    end

    writeByte(self, inReq, newState)
    return true
end

-- read pin input
function mcp23017:getPinState(bReg, pin)
    if self.deviceOk == false then
        return nil
    end

    local inReq = getGPIORegisterAddr(self, bReg)
    local inPin = checkPinIsInRange(pin)
    local response = readByte(self, inReq)
    return issetBit(response, inPin)
end

local function writeToRegister(self, registerAddr, newByte)
    if self.deviceOk == false then
        return nil
    end
    return writeByte(self, registerAddr, newByte)
end

local function readFromRegister(self, registerAddr)
    if self.deviceOk == false then
        return nil
    end
    return readByte(self, registerAddr)
end

function mcp23017:writeIODIR(bReg, newByte)
    return writeToRegister(self, getDirRegisterAddr(self, bReg), newByte)
end

function mcp23017:writeGPIO(bReg, newByte)
    return writeToRegister(self, getGPIORegisterAddr(self, bReg), newByte)
end

function mcp23017:readGPIO(bReg)
    return readFromRegister(self, getGPIORegisterAddr(self, bReg))
end

return function(address, i2cId)
    local self = {}
    setmetatable(self, mcp23017)

    -- defaults
    self.deviceOK = false
    self.i2cId = 0
    self.address = nil
    self.OUTPUT = false
    self.INPUT = true
    self.GPA = false
    self.GPB = true
    self.HIGH = true
    self.LOW = false

    self:setup(address, i2cId)
    return self
end


