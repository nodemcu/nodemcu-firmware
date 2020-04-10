--[[

	This Lua module provides access to the MCP23017 module.

    The MCP23017 is a port expander and provides 16 channels for inputs and outputs.
    Up to 8 devices (128 channels) are possible by the configurable address (A0 - A2 Pins).

    Due to the 16 channels, 2 bytes are required for switching outputs or reading input signals. These are A and B.
    A single pin can be set or a whole byte. The numbering of the individual pins starts at 0 and ends with 15.
    The numbers up to 7 are on register GPIO A and from 8 to 15 on GPIO B.

	The module requires `i2c` and `bit` C module built into firmware.


	@author Marcel P. | Plomi.net
	@github https://github.com/plomi-net

	@version 1.0.0

]]

local i2c, string = i2c, string
local issetBit = bit.isset
local setBit = bit.set
local clearBit = bit.clear

local MCP23017_ADDRESS = 0x20 -- (0x20 to 0x27)
-- MCP23017_SDA = 2 -- D2/GPIO 4
-- MCP23017_SCL = 1 -- D1/GPIO 5

local OUTPUT = false
local INPUT = true
local HIGH = true
local LOW = false

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

-- internal
local MCP23017_id = 0
local MCP23017_SPEED = i2c.SLOW
local MCP23017_DEVICE_OK = false

-- check device is available on address
local function checkDevice(address)
    i2c.start(MCP23017_id)
    local response = i2c.address(MCP23017_id, address, i2c.TRANSMITTER)
    i2c.stop(MCP23017_id)
    if response ~= true then
        print("MCP23017 device on " .. string.format('0x%02X', address) .. " not found")
    end
    return response
end

-- check device address (0x20 to 0x27)
local function checkAddress(address)
    local addr = tonumber(address)
    if (addr > 31 and addr < 40) then
        return true
    else
        print("MCP23017 address is out of range")
        return false
    end
end

-- write byte
local function writeByte(registerAddr, val)
    i2c.start(MCP23017_id)
    i2c.address(MCP23017_id, MCP23017_ADDRESS, i2c.TRANSMITTER)
    i2c.write(MCP23017_id, registerAddr)
    i2c.write(MCP23017_id, val)
    i2c.stop(MCP23017_id)
end

-- read byte
local function readByte(registerAddr)
    i2c.start(MCP23017_id)
    i2c.address(MCP23017_id, MCP23017_ADDRESS, i2c.TRANSMITTER)
    i2c.write(MCP23017_id, registerAddr)
    i2c.stop(MCP23017_id)
    i2c.start(MCP23017_id)
    i2c.address(MCP23017_id, MCP23017_ADDRESS, i2c.RECEIVER)
    local data = i2c.read(MCP23017_id, 1)
    i2c.stop(MCP23017_id)
    return string.byte(data)
end

-- get IO dir register
local function getDirRegisterAddr(pin)
    if pin > 7 then
        return MCP23017_IODIRB
    else
        return MCP23017_IODIRA
    end
end

-- get GPIO register address
local function getGPIORegisterAddr(pin)
    if pin > 7 then
        return MCP23017_GPIOB
    else
        return MCP23017_GPIOA
    end
end

-- convert pin number for A/B register
local function convertPin(pin)
    if pin > 15 or pin < 0 then
        print("The pin must be between 0 and 15")
        return nil
    end
    return pin % 8
end

-- setup internal pullup
local function setInternalPullUp(iByte)
    writeByte(MCP23017_DEFVALA, iByte)
    writeByte(MCP23017_DEFVALB, iByte)
end

-- set default GPIO mode
local function setDefaultMode(iByte)
    writeByte(MCP23017_IODIRA, iByte)
    writeByte(MCP23017_IODIRB, iByte)
end

local function numberToBool(val)
    if val == 1 or val == true or val == '1' then
        return true
    else
        return false
    end
end

-- reset gpio mode and pullup
local function reset()
    setDefaultMode(0xFF) -- all to input
    setInternalPullUp(0x00) -- disable pullup
end

-- setup device
local function setup(address, sclPin, sdaPin)

    MCP23017_ADDRESS = string.format('0x%02X', address)

    i2c.setup(MCP23017_id, sdaPin, sclPin, MCP23017_SPEED)

    if (checkAddress(address) ~= true) or (checkDevice(address) ~= true) then
        MCP23017_DEVICE_OK = false
        return 0
    else
        MCP23017_DEVICE_OK = true
        reset()
        return 1
    end
end

-- set mode for a pin
local function setMode(pin, mode)
    if MCP23017_DEVICE_OK == false then
        return nil
    end

    local inReq = getDirRegisterAddr(pin)
    local inPin = convertPin(pin)
    local response = readByte(inReq)
    local newState

    if numberToBool(mode) == OUTPUT then
        newState = clearBit(response, inPin)
    else
        newState = setBit(response, inPin)
    end

    writeByte(inReq, newState)
    return true
end

-- set pin to low or high
local function setPin(pin, state)
    if MCP23017_DEVICE_OK == false then
        return nil
    end

    local inReq = getGPIORegisterAddr(pin)
    local inPin = convertPin(pin)
    local response = readByte(inReq)
    local newState

    if numberToBool(state) == HIGH then
        newState = setBit(response, inPin)
    else
        newState = clearBit(response, inPin)
    end

    writeByte(inReq, newState)
    return true
end

-- read pin input
local function getPinState(pin)
    if MCP23017_DEVICE_OK == false then
        return nil
    end

    local inReq = getGPIORegisterAddr(pin)
    local inPin = convertPin(pin)
    local response = readByte(inReq)
    return issetBit(response, inPin)
end

local function writeToRegsiter(registerAddr, newByte)
    if MCP23017_DEVICE_OK == false then
        return nil
    end
    return writeByte(registerAddr, newByte)
end

local function readFromRegsiter(registerAddr)
    if MCP23017_DEVICE_OK == false then
        return nil
    end
    return readByte(registerAddr)
end

local function writeIODIRA(newByte)
    return writeToRegsiter(MCP23017_IODIRA, newByte)
end

local function writeIODIRB(newByte)
    return writeToRegsiter(MCP23017_IODIRB, newByte)
end

local function writeGPIOA(newByte)
    return writeToRegsiter(MCP23017_GPIOA, newByte)
end

local function writeGPIOB(newByte)
    return writeToRegsiter(MCP23017_GPIOB, newByte)
end

local function readGPIOA()
    return readFromRegsiter(MCP23017_GPIOA)
end

local function readGPIOB()
    return readFromRegsiter(MCP23017_GPIOB)
end

-- Set module name as parameter of require and return module table
local M = {
    HIGH = HIGH,
    LOW = LOW,
    OUTPUT = OUTPUT,
    INPUT = INPUT,
    checkDevice = checkDevice,
    setup = setup,
    setMode = setMode,
    setPin = setPin,
    getPinState = getPinState,
    reset = reset,
    setInternalPullUp = setInternalPullUp,
    writeIODIRA = writeIODIRA,
    writeIODIRB = writeIODIRB,
    writeGPIOA = writeGPIOA,
    writeGPIOB = writeGPIOB,
    readGPIOA = readGPIOA,
    readGPIOB = readGPIOB,
}

_G[modname or 'mcp23017'] = M
return M
