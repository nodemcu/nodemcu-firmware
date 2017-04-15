--------------------------------------------------------------------------------
-- DS18B20 one wire module for NODEMCU
-- NODEMCU TEAM
-- LICENCE: http://opensource.org/licenses/MIT
-- @voborsky, @devsaurus, TerryE  26 Mar 2017
----------------------------------------------------------------------------------------------------------------------------------------------------------------
local modname = ...

-- Used modules and functions
local table, string, ow, tmr, print, type, tostring, pcall, ipairs = 
      table, string, ow, tmr, print, type, tostring, pcall, ipairs
-- Local functions
local ow_setup, ow_search, ow_select, ow_read, ow_read_bytes, ow_write, ow_crc8, ow_reset, ow_reset_search, ow_skip, ow_depower = 
      ow.setup, ow.search, ow.select, ow.read, ow.read_bytes, ow.write, ow.crc8, ow.reset, ow.reset_search, ow.skip, ow.depower
local node_task_post, node_task_LOW_PRIORITY = node.task.post, node.task.LOW_PRIORITY
local string_char, string_dump = string.char, string.dump
local now, tmr_create, tmr_ALARM_SINGLE = tmr.now, tmr.create, tmr.ALARM_SINGLE
local table_sort, table_concat = table.sort, table.concat
local math_floor = math.floor
local file_open = file.open

table, string, tmr, ow = nil, nil, nil, nil

local DS18B20FAMILY   = 0x28
local DS1920FAMILY    = 0x10  -- and DS18S20 series
local CONVERT_T       = 0x44
local READ_SCRATCHPAD = 0xBE
local READ_POWERSUPPLY= 0xB4
local MODE = 1

local pin, cb, unit = 3
local status = {}

local debugPrint = function() return end

--------------------------------------------------------------------------------
-- Implementation
--------------------------------------------------------------------------------
local function enable_debug()
  debugPrint = function (...) print(now(),' ', ...) end
end

local function to_string(addr, esc)
  if type(addr) == 'string' and #addr == 8 then
    return ( esc == true and
             '"\\%u\\%u\\%u\\%u\\%u\\%u\\%u\\%u"' or
             '%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X '):format(addr:byte(1,8)) 
  else
    return tostring(addr)
  end
end

local function readout(self)
  local next = false
  local sens = self.sens
  local temp = self.temp
  for i, s in ipairs(sens) do
    if status[i] == 1 then
      ow_reset(pin)
      local addr = s:sub(1,8)
      ow_select(pin, addr)   -- select the  sensor
      ow_write(pin, READ_SCRATCHPAD, MODE)
      data = ow_read_bytes(pin, 9)

      local t=(data:byte(1)+data:byte(2)*256)
      -- t is actually signed so process the sign bit and adjust for fractional bits
      -- the DS18B20 family has 4 fractional bits and the DS18S20s, 1 fractional bit
      t = ((t <= 32767) and t or t - 65536) * 
          ((addr:byte(1) == DS18B20FAMILY) and 625 or 5000) 
      
      if 1/2 == 0 then
        -- integer version
        if unit == 'F' then
          t = (t * 18)/10 + 320000
        elseif unit == 'K' then
          t = t + 2731500
        end      
        local sgn = t<0 and -1 or 1
        local tA = sgn*t
        local tH=tA/10000
        local tL=(tA%10000)/1000 + ((tA%1000)/100 >= 5 and 1 or 0)
              
        if tH and (t~=850000) then
          temp[addr]=(sgn<0 and "-" or "")..tH.."."..tL
          debugPrint(to_string(addr),(sgn<0 and "-" or "")..tH.."."..tL)
          status[i] = 2
        end
        -- end integer version
      else
        -- float version
        if t and (math_floor(t/10000)~=85) then
          t = t / 10000
          if unit == 'F' then
            t = t * 18/10 + 32
          elseif unit == 'K' then
            t = t + 27315/100
          end      
          self.temp[addr]=t
          debugPrint(to_string(addr), t)
          status[i] = 2
        end
        -- end float version
      end
    end
    next = next or status[i] == 0
  end
  if next then 
    node_task_post(node_task_LOW_PRIORITY, function() return conversion(self) end)
  else
    --sens = {}
    if cb then
      node_task_post(node_task_LOW_PRIORITY, function() return cb(temp) end) 
    end
  end
end

local function conversion(self)
  local sens = self.sens
  local powered_only = true
  for _, s in ipairs(sens) do powered_only = powered_only and s:byte(9) ~= 1 end
  if powered_only then
    debugPrint("starting conversion: all sensors")
    ow_reset(pin)
    ow_skip(pin)  -- select the sensor
    ow_write(pin, CONVERT_T, MODE)  -- and start conversion
    for i, s in ipairs(sens) do status[i] = 1 end
  else
    for i, s in ipairs(sens) do
      if status[i] == 0 then
        local addr, parasite = s:sub(1,8), s:byte(9)
        debugPrint("starting conversion:", to_string(addr), parasite == 1 and "parasite" or " ")
        ow_reset(pin)
        ow_select(pin, addr)  -- select the sensor
        ow_write(pin, CONVERT_T, MODE)  -- and start conversion
        status[i] = 1
        if parasite == 1 then break end -- parasite sensor blocks bus during conversion
      end
    end
  end
  tmr_create():alarm(750, tmr_ALARM_SINGLE, function() return readout(self) end)
end

local function _search(self, lcb, lpin, search, save)
  self.temp = {}
  if search then self.sens = {}; status = {} end
  local temp = self.temp
  local sens = self.sens 
  pin = lpin or pin
  
  local addr
  if not search and #sens == 0 then
    -- load addreses if available
    debugPrint ("geting addreses from flash")
    local s,check,a = pcall(dofile, "ds18b20_save.lc")
    if s and check == "ds18b20" then 
      for i = 1, #a do sens[i] = a[i] end
    end
    debugPrint (#sens, "addreses found")
  end
  
  ow_setup(pin)
  if search or #sens == 0 then
    ow_reset_search(pin)
    -- ow_target_search(pin,0x28)
    -- search the first device
    addr = ow_search(pin)
  else
    for i, s in ipairs(sens) do status[i] = 0 end
  end
  local function cycle()
    debugPrint("cycle")
    if addr then
      local crc=ow_crc8(addr:sub(1,7))
      if (crc==addr:byte(8)) and ((addr:byte(1)==DS1920FAMILY) or (addr:byte(1)==DS18B20FAMILY)) then
        ow_reset(pin)
        ow_select(pin, addr)
        ow_write(pin, READ_POWERSUPPLY, MODE)
        local parasite = (ow_read(pin)==0 and 1 or 0)
        sens[#sens+1]= addr..string_char(parasite) -- {addr=addr, parasite=parasite, status=0}
        debugPrint("contact: ", to_string(addr), parasite == 1 and "parasite" or " ")
      end
      addr = ow_search(pin)
      node_task_post(node_task_LOW_PRIORITY, cycle)
    else
      ow_depower(pin)
      -- place powered sensors first
      table_sort(sens, function(a, b) return a:byte(9)<b:byte(9) end) -- parasite
      -- save sensor addreses
      if save then
        debugPrint ("saving addreses to flash")
        
        local addr_list = {}
        for i =1, #sens do 
          local s = sens[i]
          addr_list[i] = to_string(s:sub(1,8), true)..('.."\\%u"'):format(s:byte(9))
        end
        local save_statement = 'return "ds18b20", {' .. table_concat(addr_list, ',') .. '}'        
        debugPrint (save_statement)
        local save_file = file_open("ds18b20_save.lc","w")
        save_file:write(string_dump(loadstring(save_statement)))
        save_file:close()
      end
      -- end save sensor addreses
      if lcb then node_task_post(node_task_LOW_PRIORITY, lcb) end
    end
  end
  cycle()
end

local function read_temp(self, lcb, lpin, lunit, force_search, save_search)
  cb, unit = lcb, lunit or unit
  _search(self, function() return conversion(self) end, lpin, force_search, save_search)
end
 
 -- Set module name as parameter of require and return module table
local M = {
  sens = {},
  temp = {},
  C = 'C', F = 'F', K = 'K',
  read_temp = read_temp, enable_debug = enable_debug
}
_G[modname or 'ds18b20'] = M
return M
