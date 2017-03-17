--------------------------------------------------------------------------------
-- DS18B20 one wire module for NODEMCU
-- by @voborsky, @devsaurus
-- encoder module is needed only for debug output; lines can be removed if no
-- debug output is needed and/or encoder module is missing
--
-- by default the module is for integer version, comment integer version and
-- uncomment float version part for float version
--------------------------------------------------------------------------------

return({
  pin=3,
  sens={},
  temp={},

  conversion = function(self)
    local pin = self.pin
    for i,s in ipairs(self.sens) do
      if s.status == 0 then
        print("starting conversion:", encoder.toHex(s.addr), s.parasite == 1 and "parasite" or " ")
        ow.reset(pin)
        ow.select(pin, s.addr)  -- select the sensor
        ow.write(pin, 0x44, 1)  -- and start conversion
        s.status = 1
        if s.parasite == 1 then break end -- parasite sensor blocks bus during conversion
      end
    end
    tmr.create():alarm(750, tmr.ALARM_SINGLE, function() self:readout() end)
  end,

  readTemp = function(self, cb, lpin)
    if lpin then self.pin = lpin end
    local pin = self.pin
    self.cb = cb
    self.temp={}
    ow.setup(pin)

    self.sens={}
    ow.reset_search(pin)
    -- ow.target_search(pin,0x28)
    -- search the first device
    local addr = ow.search(pin)
    -- and loop through all devices
    while addr do
      -- search next device
      local crc=ow.crc8(string.sub(addr,1,7))
      if (crc==addr:byte(8)) and ((addr:byte(1)==0x10) or (addr:byte(1)==0x28)) then
        ow.reset(pin)
        ow.select(pin, addr)   -- select the found sensor
        ow.write(pin, 0xB4, 1) -- Read Power Supply [B4h]
        local parasite = (ow.read(pin)==0 and 1 or 0)
        table.insert(self.sens,{addr=addr, parasite=parasite, status=0})
        print("contact: ", encoder.toHex(addr), parasite == 1 and "parasite" or " ")
      end

      addr = ow.search(pin)
    end

    -- place powered sensors first
    table.sort(self.sens, function(a,b) return a.parasite<b.parasite end)

    node.task.post(node.task.MEDIUM_PRIORITY, function() self:conversion() end)
  end,

  readout=function(self)
    local pin = self.pin
    local next = false
        if not self.sens then return 0 end
    for i,s in ipairs(self.sens) do
      -- print(encoder.toHex(s.addr), s.status)
      if s.status == 1 then
        ow.reset(pin)
        ow.select(pin, s.addr)   -- select the  sensor
        ow.write(pin, 0xBE, 0) -- READ_SCRATCHPAD
        data = ow.read_bytes(pin, 9)

        local t=(data:byte(1)+data:byte(2)*256)
        if (t > 0x7fff) then t = t - 0x10000 end
        if (s.addr:byte(1) == 0x28) then
          t = t * 625  -- DS18B20, 4 fractional bits
        else
          t = t * 5000 -- DS18S20, 1 fractional bit
        end

        if 1/2 == 0 then
          -- integer version
          local sgn = t<0 and -1 or 1
          local tA = sgn*t
          local tH=tA/10000
          local tL=(tA%10000)/1000 + ((tA%1000)/100 >= 5 and 1 or 0)

          if tH and (tH~=85) then
            self.temp[s.addr]=(sgn<0 and "-" or "")..tH.."."..tL
            print(encoder.toHex(s.addr),(sgn<0 and "-" or "")..tH.."."..tL)
            s.status = 2
          end
          -- end integer version
        else
          -- float version
          if t and (math.floor(t/10000)~=85) then
            self.temp[s.addr]=t/10000
            print(encoder.toHex(s.addr), t)
            s.status = 2
          end
          -- end float version
        end
      end
      next = next or s.status == 0
    end
    if next then
      node.task.post(node.task.MEDIUM_PRIORITY, function() self:conversion()  end)
    else
      self.sens = nil
      if self.cb then
        node.task.post(node.task.MEDIUM_PRIORITY, function()  self.cb(self.temp) end)
      end
    end

  end
})
