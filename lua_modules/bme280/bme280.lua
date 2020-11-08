--[[
 BME280 Lua module
 Requires i2c and bme280_math module
 Written by Lukas Voborsky, @voborsky
]]

local bme280 = {}

-- bme280.setup()
-- bme280:setup()
-- bme280:read()
-- bme280:altitude()
-- bme280:dewpoint()
-- bme280:qfe2qnh()
-- bme280:startreadout()

local type, assert = type, assert
local table_concat, math_floor = table.concat, math.floor
local i2c_start, i2c_stop, i2c_address, i2c_read, i2c_write, i2c_TRANSMITTER, i2c_RECEIVER =
  i2c.start, i2c.stop, i2c.address, i2c.read, i2c.write, i2c.TRANSMITTER, i2c.RECEIVER
local bme280_math_setup, bme280_math_read, bme280_math_qfe2qnh, bme280_math_altitude, bme280_math_dewpoint =
  bme280_math.setup, bme280_math.read, bme280_math.qfe2qnh, bme280_math.altitude, bme280_math.dewpoint
local tmr_create, tmr_ALARM_SINGLE = tmr.create, tmr.ALARM_SINGLE


local BME280_I2C_ADDRESS1 = 0x76
local BME280_I2C_ADDRESS2 = 0x77

local BME280_REGISTER_CONTROL = 0xF4
local BME280_REGISTER_CONTROL_HUM = 0xF2
local BME280_REGISTER_CONFIG= 0xF5
local BME280_REGISTER_CHIPID = 0xD0

local BME280_REGISTER_DIG_T = 0x88  -- 0x88-0x8D ( 6)
local BME280_REGISTER_DIG_P = 0x8E  -- 0x8E-0x9F (18)
local BME280_REGISTER_DIG_H1 = 0xA1  -- 0xA1      ( 1)
local BME280_REGISTER_DIG_H2 = 0xE1  -- 0xE1-0xE7 ( 7)
local BME280_REGISTER_PRESS = 0xF7	 -- 0xF7-0xF9

-- local BME280_FORCED_MODE = 0x01

-- maximum measurement time in ms for maximum oversampling for all measures
-- 113 > 1.25 + 2.3*16 + 2.3*16 + 0.575 + 2.3*16 + 0.575 ms
local BME280_SAMPLING_DELAY =113

-- Local functions
local read_reg
local write_reg
local bme280_setup
local bme280_read
local bme280_startreadout

-- -- Note that the space between debug and the arglist is there for a reason
-- -- so that a simple global edit "   debug(" -> "-- debug(" or v.v. to
-- -- toggle debug compiled into the module.
-- local print, node_heap = print, node.heap
-- local function debug (fmt, ...) -- upval: cnt (, print, node_heap)
  -- if not bme280.debug then return end
  -- if (...) then fmt = fmt:format(...) end
  -- print("[bme280]", node_heap(), fmt)
-- end

--------------------------- Set up the bme280 object ----------------------------
--       bme280 has method setup to create the sensor object and setup the sensor
--       object created by bme280.setup() has methods: read, qfe2qnh, altitude, dewpoint
---------------------------------------------------------------------------------

function bme280.setup(id, addr, temp_oss, press_oss, humi_oss, power_mode, inactive_duration, IIR_filter, full_init)
  return bme280_setup(nil, id,
    addr, temp_oss, press_oss, humi_oss, power_mode, inactive_duration, IIR_filter, full_init)
end

------------------------------------------------------------------------------
function bme280_setup(self, id, addr,
  temp_oss, press_oss, humi_oss, power_mode, inactive_duration, IIR_filter, full_init)

  addr = (addr==2) and BME280_I2C_ADDRESS2 or BME280_I2C_ADDRESS1
  full_init = full_init or true

  -- debug("%d %x %d", id, addr, BME280_REGISTER_CHIPID)
  local chipid = read_reg(id, addr, BME280_REGISTER_CHIPID, 1)
  if not chipid then
    return nil
  end
  -- debug("chip_id: %x", chipid:byte(1))
  local isbme = (chipid:byte(1) == 0x60)

  local buf = {}
  buf[1] = read_reg(id, addr, BME280_REGISTER_DIG_T, 6)
  buf[2] = read_reg(id, addr, BME280_REGISTER_DIG_P, 18)
  if (isbme) then
    buf[3] = read_reg(id, addr, BME280_REGISTER_DIG_H1, 1)
    buf[4] = read_reg(id, addr, BME280_REGISTER_DIG_H2, 7)
  end

  local sensor, config = bme280_math_setup(table_concat(buf),
    temp_oss, press_oss, humi_oss, power_mode, inactive_duration, IIR_filter)
  self = self  or {
    setup = bme280_setup,
    read = bme280_read,
    startreadout = bme280_startreadout,
    qfe2qnh = bme280_math_qfe2qnh,
    altitude = bme280_math_altitude,
    dewpoint = bme280_math_dewpoint
  }
  self.id, self.addr = id, addr
  self._sensor, self._config, self._isbme = sensor, config, isbme

  if (full_init) then
    write_reg(id, addr, BME280_REGISTER_CONFIG, config[1])
    if (isbme) then write_reg(id, addr, BME280_REGISTER_CONTROL_HUM, config[2]) end
    write_reg(id, addr, BME280_REGISTER_CONTROL, config[3])
  end

  return self
end

function bme280_read(self, alt)
  local buf = read_reg(self.id, self.addr, BME280_REGISTER_PRESS, 8) -- registers are P[3], T[3], H[2]
  if buf then
    return bme280_math_read(self._sensor, buf, alt)
  else
    return nil
  end
end

function bme280_startreadout(self, callback, delay, alt)
  assert(type(callback) == "function", "invalid callback parameter")

  delay = delay or BME280_SAMPLING_DELAY

  if self._isbme then write_reg(self.id, self.addr, BME280_REGISTER_CONTROL_HUM, self._config[2]) end
  write_reg(self.id, self.addr, BME280_REGISTER_CONTROL, 4*math_floor(self._config[3]:byte(1)/4)+ 1)
    -- 4*math_floor(self._config[3]:byte(1)/4)+ 1
    --   an awful way to avoid bit operations but calculate (config[3] & 0xFC) | BME280_FORCED_MODE
    -- Lua 5.3 integer division // would be more suitable

  tmr_create():alarm(delay, tmr_ALARM_SINGLE,
    function()
      callback(bme280_read(self, alt))
    end
  )
end

function write_reg(id, dev_addr, reg_addr, data)
  i2c_start(id)
  if not i2c_address(id, dev_addr, i2c_TRANSMITTER) then
    -- debug("No ACK on address: %x", dev_addr)
    return nil
  end
  i2c_write(id, reg_addr)
  local c = i2c_write(id, data)
  i2c_stop(id)
  return c
end

function read_reg(id, dev_addr, reg_addr, n)
  i2c_start(id)
  if not i2c_address(id, dev_addr, i2c_TRANSMITTER) then
    -- debug("No ACK on address: %x", dev_addr)
    return nil
  end
  i2c_write(id, reg_addr)
  i2c_stop(id)
  i2c_start(id)
  i2c_address(id, dev_addr, i2c_RECEIVER)
  local c = i2c_read(id, n)
  i2c_stop(id)
  return c
end

------------------------------------------------ -----------------------------
return bme280
