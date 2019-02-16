-- ****************************************************************************
--
-- Compatability wrapper for mapping ESP8266's spi module API to ESP32
--
-- Usage:
--
--   spi = require("spi_compat")([pin_sclk], [pin_mosi], [pin_miso], [pin_cs])
--
--     pin_sclk: GPIO pin for SCLK, optional
--     pin_mosi: GPIO pin for MOSI, optional
--     pin_miso: GPIO pin for MISO, optional
--     pin_cs:   GPIO pin for CS, optional
--
-- ****************************************************************************

local M = {}

local _pin_sclk, _pin_mosi, _pin_miso, _pin_cs

local _spi

local _duplex
local _device


-- ****************************************************************************
-- Implement esp8266 compatability API
--
function M.setup(id, mode, cpol, cpha, databits, clock_div, duplex_mode)
  if databits ~= 8 then
    error("only 8 bits per item supported")
  end

  local bus_master = _spi.master(_spi.HSPI, {sclk = _pin_sclk, mosi = _pin_mosi, miso = _pin_miso})

  local dev_config = {}
  dev_config.cs = _pin_cs
  dev_config.mode = cpol * 2 + cpha
  dev_config.freq = 80000000 / clock_div

  _device = bus_master:device(dev_config)

  _duplex = duplex_mode or M.HALFDUPLEX

end

function M.send(id, ...)
  local results = {}
  local wrote = 0

  for idx = 1, select("#", ...) do
    local arg = select(idx, ...)
    if type(arg) == "number" then
      table.insert(results, _device:transfer(string.char(arg)):byte(1))
      wrote = wrote + 1
    elseif type(arg) == "string" then
      table.insert(results, _device:transfer(arg))
      wrote = wrote + #arg
    elseif type(arg) == "table" then
      local rtab = {}
      for i, data in ipairs(arg) do
        table.insert(rtab, _device:transfer(string.char(data)):byte(1))
        wrote = wrote + 1
      end
      table.insert(results, rtab)
    else
      error("wrong argument type")
    end
  end

  if _duplex == M.FULLDUPLEX then
    return wrote, unpack(results)
  else
    return wrote
  end
end

function M.recv(id, size, default_data)
  local def = default_data or 0xff
  return _device:transfer(string.char(def):rep(size))
end


return function (pin_sclk, pin_mosi, pin_miso, pin_cs)
  -- cache built-in module
  _spi = spi
  -- invalidate built-in module
  spi = nil

  -- forward unchanged functions

  -- forward constant definitions
  M.MASTER = 0
  M.CPOL_LOW  = 0
  M.CPOL_HIGH = 1
  M.CPHA_LOW  = 0
  M.CPHA_HIGH = 1
  M.HALFDUPLEX = 0
  M.FULLDUPLEX = 1
  M.DATABITS_8 = 8
  
  _pin_sclk = pin_sclk
  _pin_mosi = pin_mosi
  _pin_miso = pin_miso
  _pin_cs   = pin_cs

  return M
end
