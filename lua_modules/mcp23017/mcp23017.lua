---
-- @description Expands the ESP8266's GPIO to 8 more I/O pins via I2C with the MCP23017 I/O Expander
-- MCP23017 Datasheet: http://ww1.microchip.com/downloads/en/DeviceDoc/21919e.pdf
-- Tested on NodeMCU 0.9.5 build 20150213.
-- @date March 02, 2015
-- @author Miguel 
--  GitHub: https://github.com/AllAboutEE 
--  YouTube: https://www.youtube.com/user/AllAboutEE
--  Website: http://AllAboutEE.com
--------------------------------------------------------------------------------

local moduleName = ... 
local M = {}
_G[moduleName] = M 

-- Constant device address.
local MCP23017_ADDRESS = 0x20

-- These address are valid for IOCON.BANK = 0
-- Do not change IOCON.BANK

local MCP23017_IODIRA = 0x00
local MCP23017_IODIRB = 0x1
local MCP23017_IPOLA = 0x02
local MCP23017_IPOLB = 0x03
local MCP23017_GPINTENA = 0x04
local MCP23017_GPINTENB = 0x05
local MCP23017_DEFVALA = 0x06
local MCP23017_DEFVALB = 0x07
local MCP23017_INTCONA = 0x08
local MCP23017_INTCONB = 0x09
local MCP23017_IOCON = 0x0A
local MCP23017_IOCON = 0x0B
local MCP23017_GPPUA = 0x0C
local MCP23017_GPPUB = 0x0D
local MCP23017_INTFA = 0x0E
local MCP23017_INTFB = 0x0F
local MCP23017_INTCAPA = 0x10
local MCP23017_INTCAPB = 0x11
local MCP23017_GPIOA = 0x12
local MCP23017_GPIOB = 0x13
local MCP23017_OLATA = 0x14
local MCP23017_OLATB = 0x15

-- Default value for i2c communication
local id = 0

-- pin modes for I/O direction
M.INPUT = 1
M.OUTPUT = 0

-- pin states for I/O i.e. on/off
M.HIGH = 1
M.LOW = 0

-- Weak pull-up resistor state
M.ENABLE = 1
M.DISABLE = 0


---
-- @name write
-- @description Writes one byte to a register
-- @param registerAddress The register where we'll write data
-- @param data The data to write to the register
-- @return void
----------------------------------------------------------
local function write(registerAddress, data)
    i2c.start(id)
    i2c.address(id,MCP23017_ADDRESS,i2c.TRANSMITTER) -- send MCP's address and write bit
    i2c.write(id,registerAddress)
    i2c.write(id,data)
    i2c.stop(id)
end

---
-- @name read
-- @description Reads the value of a regsiter
-- @param registerAddress The reigster address from which to read
-- @return The byte stored in the register
----------------------------------------------------------
local function read(registerAddress)
    -- Tell the MCP which register you want to read from
    i2c.start(id)
    i2c.address(id,MCP23017_ADDRESS,i2c.TRANSMITTER) -- send MCP's address and write bit
    i2c.write(id,registerAddress)
    i2c.stop(id)
    i2c.start(id)
    -- Read the data form the register
    i2c.address(id,MCP23017_ADDRESS,i2c.RECEIVER) -- send the MCP's address and read bit
    local data = 0x00
    data = i2c.read(id,1) -- we expect only one byte of data
    i2c.stop(id)

    return string.byte(data) -- i2c.read returns a string so we convert to it's int value
end

---
--! @name begin
--! @description Sets the MCP23017 device address's last three bits. 
--  Note: The address is defined as binary 0100[A2][A1][A0] where 
--  A2, A1, and A0 are defined by the connection of the pins, 
--  e.g. if the pins are connected all to GND then the paramter address 
--  will need to be 0x0.
-- @param address The 3 least significant bits (LSB) of the address
-- @param pinSDA The pin to use for SDA
-- @param pinSCL The pin to use for SCL
-- @param speed The speed of the I2C signal
-- @return void
---------------------------------------------------------------------------
function M.begin(address,pinSDA,pinSCL,speed)
 MCP23017_ADDRESS = bit.bor(MCP23017_ADDRESS,address)
 i2c.setup(id,pinSDA,pinSCL,speed)
end

---
-- @name writeGPIO
-- @description Writes a byte of data to the GPIO register
-- @param dataByte The byte of data to write
-- @return void
----------------------------------------------------------
function M.writeGPIOA(dataByte)
    write(MCP23017_GPIOA,dataByte)
end

function M.writeGPIOB(dataByte)
    write(MCP23017_GPIOB,dataByte)
end

---
-- @name readGPIO
-- @description reads a byte of data from the GPIO register
-- @return One byte of data
----------------------------------------------------------
function M.readGPIOA()
    return read(MCP23017_GPIOA)
end

function M.readGPIOB()
    return read(MCP23017_GPIOB)
end

---
-- @name writeIODIR
-- @description Writes one byte of data to the IODIR register
-- @param dataByte The byte of data to write
-- @return void
----------------------------------------------------------
function M.writeIODIRA(dataByte)
    write(MCP23017_IODIRA,dataByte)
end

function M.writeIODIRB(dataByte)
    write(MCP23017_IODIRB,dataByte)
end

---
-- @name readIODIR
-- @description Reads a byte from the IODIR register
-- @return The byte of data in IODIR
----------------------------------------------------------
function M.readIODIRA()
    return read(MCP23017_IODIRA)
end

function M.readIODIRB()
    return read(MCP23017_IODIRB)
end

---
-- @name writeGPPU The byte to write to the GPPU
-- @description Writes a byte of data to the 
-- PULL-UP RESISTOR CONFIGURATION (GPPU)REGISTER 
-- @param databyte the value to write to the GPPU register.
-- each bit in this byte is assigned to an individual GPIO pin
-- @return void
----------------------------------------------------------------
function M.writeGPPUA(dataByte)
    write(MCP23017_GPPUA,dataByte)
end

function M.writeGPPUB(dataByte)
    write(MCP23017_GPPUB,dataByte)
end

---
-- @name readGPPU
-- @description Reads the GPPU (Pull-UP resistors register) byte
-- @return The GPPU byte i.e. state of all internal pull-up resistors
-------------------------------------------------------------------
function M.readGPPUA()
    return read(MCP23017_GPPUA)
end

function M.readGPPUB()
    return read(MCP23017_GPPUB)
end

return M
