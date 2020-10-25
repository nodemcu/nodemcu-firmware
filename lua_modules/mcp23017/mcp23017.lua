--[[
    This Lua module provides access to the MCP23017 module.

    The MCP23017 is a port expander and provides 16 channels for inputs and
    outputs.  Up to 8 devices (128 channels) are possible by the configurable
    address (A0 - A2 Pins).

    Due to the 16 channels, 2 bytes are required for switching outputs or
    reading input signals. These are A and B.  A single pin can be set or a
    whole byte.

    The numbering of the individual pins starts at 0 and ends with 7.  The
    numbers are for each register GPIO A and GPIO B.

    The module requires `i2c` and `bit` C module built into firmware.

    For register name <-> number mapping, see the MCP23017 data sheet, table
    3-1 on page 12 (as of Revision C, July 2016).  This module uses the
    IOCON.BANK=0 numbering (the default) throughout.

    @author Marcel P. | Plomi.net
    @github https://github.com/plomi-net
    @version 1.0.0
]]

local i2c, string, issetBit, setBit, clearBit =
      i2c, string, bit.isset, bit.set, bit.clear

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

-- metatable
local mcp23017 = {
    -- convenience parameter enumeration names
    INPUT = true,
    OUTPUT = false,
    GPA = false,
    GPB = true,
    HIGH = true,
    LOW = false
}
mcp23017.__index = mcp23017

-- check device is available on address
local function checkDevice(address, i2cId)
    i2c.start(i2cId)
    local response = i2c.address(i2cId, address, i2c.TRANSMITTER)
    i2c.stop(i2cId)
    return response
end

-- write byte
local function writeByte(address, i2cId, registerAddr, val)
    i2c.start(i2cId)
    i2c.address(i2cId, address, i2c.TRANSMITTER)
    i2c.write(i2cId, registerAddr)
    i2c.write(i2cId, val)
    i2c.stop(i2cId)
end

-- read byte
local function readByte(address, i2cId, registerAddr)
    i2c.start(i2cId)
    i2c.address(i2cId, address, i2c.TRANSMITTER)
    i2c.write(i2cId, registerAddr)
    i2c.stop(i2cId)
    i2c.start(i2cId)
    i2c.address(i2cId, address, i2c.RECEIVER)
    local data = i2c.read(i2cId, 1)
    i2c.stop(i2cId)
    return string.byte(data)
end

-- check pin is in range
local function checkPinIsInRange(pin)
    if pin > 7 or pin < 0 then
        error("MCP23017 the pin must be between 0 and 7")
    end
    return pin
end

function mcp23017:writeIODIR(bReg, newByte)
    writeByte(self.address, self.i2cId,
        bReg and MCP23017_IODIRB or MCP23017_IODIRA, newByte)
end

function mcp23017:writeGPIO(bReg, newByte)
    writeByte(self.address, self.i2cId,
        bReg and MCP23017_GPIOB or MCP23017_GPIOA, newByte)
end

function mcp23017:readGPIO(bReg)
    return readByte(self.address, self.i2cId,
        bReg and MCP23017_GPIOB or MCP23017_GPIOA)
end

-- read pin input
function mcp23017:getPinState(bReg, pin)
    return issetBit(readByte(self.address, self.i2cId,
        bReg and MCP23017_GPIOB or MCP23017_GPIOA),
        checkPinIsInRange(pin))
end

-- set pin to low or high
function mcp23017:setPin(bReg, pin, state)
    local a, i = self.address, self.i2cId
    local inReq = bReg and MCP23017_GPIOB or MCP23017_GPIOA
    local inPin = checkPinIsInRange(pin)
    local response = readByte(a, i, inReq)
    writeByte(a, i, inReq,
        state and setBit(response, inPin) or clearBit(response, inPin))
    return true
end

-- set mode for a pin
function mcp23017:setMode(bReg, pin, mode)
    local a, i = self.address, self.i2cId
    local inReq = bReg and MCP23017_IODIRB or MCP23017_IODIRA
    local inPin = checkPinIsInRange(pin)
    local response = readByte(a, i, inReq)
    writeByte(a, i, inReq,
        mode and setBit(response, inPin) or clearBit(response, inPin))
    return true
end

-- reset gpio mode
function mcp23017:reset()
    local a, i = self.address, self.i2cId
    writeByte(a, i, MCP23017_IODIRA, 0xFF)
    writeByte(a, i, MCP23017_IODIRB, 0xFF)
end

-- setup internal pullup
function mcp23017:setInternalPullUp(bReg, iByte)
    writeByte(self.address, self.i2cId,
        bReg and MCP23017_DEFVALB or MCP23017_DEFVALA, iByte)
end

return function(address, i2cId)
    local self = setmetatable({}, mcp23017)

    -- check device address (0x20 to 0x27)
    if (address < 32 or address > 39) then
        error("MCP23017 address is out of range")
    end

    if (checkDevice(address, i2cId) ~= true) then
        error("MCP23017 device on " .. string.format('0x%02X', address) .. " not found")
    end

    self.address = address
    self.i2cId = i2cId

    self:reset()
    return self
end


