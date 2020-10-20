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

local i2c, string, issetBit, setBit, clearBit, error = i2c, string, bit.isset, bit.set, bit.clear, error
local isINPUT, isGPB, isHIGH = true, true, true

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
    INPUT = isINPUT,
    OUTPUT = not isINPUT,
    GPA = not isGPB,
    GPB = isGPB,
    HIGH = isHIGH,
    LOW = not isHIGH
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

local function reset(address, i2cId)
    writeByte(address, i2cId, MCP23017_IODIRA, 0xFF)
    writeByte(address, i2cId, MCP23017_IODIRB, 0xFF)
end

-- setup device
local function setup(address, i2cId)

    -- check device address (0x20 to 0x27)
    if (address < 32 or address > 39) then
        error("MCP23017 address is out of range")
    end

    if (checkDevice(address, i2cId) ~= true) then
        error("MCP23017 device on " .. string.format('0x%02X', address) .. " not found")
    else
        reset(address, i2cId)
        return 1
    end
end

return function(address, i2cId)
    local self = setmetatable({}, mcp23017)

    if setup(address, i2cId) then
        self.writeIODIR = function(sf, bReg, newByte) -- luacheck: no unused
            writeByte(address, i2cId,
                bReg == isGPB and MCP23017_IODIRB or MCP23017_IODIRA,
                newByte)
        end

        self.writeGPIO = function(sf, bReg, newByte) -- luacheck: no unused
            writeByte(address, i2cId,
                bReg == isGPB and MCP23017_GPIOB or MCP23017_GPIOA, newByte)
        end

        self.readGPIO = function(sf, bReg) -- luacheck: no unused
            return readByte(address, i2cId, -- upvals
                bReg == isGPB and MCP23017_GPIOB or MCP23017_GPIOA)
        end

        -- read pin input
        self.getPinState = function(sf, bReg, pin) -- luacheck: no unused
            return issetBit(readByte(address, i2cId,
                bReg == isGPB and MCP23017_GPIOB or MCP23017_GPIOA),
                checkPinIsInRange(pin))
        end

        -- set pin to low or high
        self.setPin = function(sf, bReg, pin, state) -- luacheck: no unused
            local inReq = bReg == isGPB and MCP23017_GPIOB or MCP23017_GPIOA
            local inPin = checkPinIsInRange(pin)
            local response = readByte(address, i2cId, inReq)
            writeByte(address, i2cId, inReq,
                state == isHIGH and setBit(response, inPin) or clearBit(response, inPin))
            return true
        end

        -- set mode for a pin
        self.setMode = function(sf, bReg, pin, mode) -- luacheck: no unused
            local inReq = bReg == isGPB and MCP23017_IODIRB or MCP23017_IODIRA
            local inPin = checkPinIsInRange(pin)
            local response = readByte(address, i2cId, inReq)
            writeByte(address, i2cId, inReq,
                mode == isINPUT and setBit(response, inPin) or clearBit(response, inPin))
            return true
        end

        -- reset gpio mode
        self.reset = function(sf) -- luacheck: no unused
            reset(address, i2cId)
        end

        -- setup internal pullup
        self.setInternalPullUp = function(sf, bReg, iByte) -- luacheck: no unused
            writeByte(address, i2cId,
                bReg == isGPB and MCP23017_DEFVALB or MCP23017_DEFVALA, iByte)
        end

        return self
    end
    return nil
end


