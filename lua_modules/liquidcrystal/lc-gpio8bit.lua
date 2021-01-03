local gpio, bit = gpio, bit --luacheck: read globals gpio bit

return function(bus_args)
   local rs = bus_args.rs or 0
   local rw = bus_args.rw
   local en = bus_args.en or 1
   local bl = bus_args.backlight
   local d0 = bus_args.d0 or 2
   local d1 = bus_args.d1 or 3
   local d2 = bus_args.d2 or 4
   local d3 = bus_args.d3 or 5
   local d4 = bus_args.d4 or 6
   local d5 = bus_args.d5 or 7
   local d6 = bus_args.d6 or 8
   local d7 = bus_args.d7 or 9

   for _, d in pairs({rs,rw,en,bl}) do
      if d then
         gpio.mode(d, gpio.OUTPUT)
      end
   end

   local function setGPIO(mode)
      for _, d in pairs({d0, d1, d2, d3, d4, d5, d6, d7}) do
         gpio.mode(d, mode)
      end
   end

   setGPIO(gpio.OUTPUT)

   local function send8bitGPIO(value, rs_en, rw_en, read)
      local function exchange(data)
         local rv = 0
         if rs then gpio.write(rs, rs_en and gpio.HIGH or gpio.LOW) end
         if rw then gpio.write(rw, rw_en and gpio.HIGH or gpio.LOW) end
         gpio.write(en, gpio.HIGH)
         for i, d in ipairs({d0, d1, d2, d3, d4, d5, d6, d7}) do
            if read and rw then
               if gpio.read(d) == 1 then rv = bit.set(rv, i-1) end
            else
               gpio.write(d, bit.isset(data, i-1) and gpio.HIGH or gpio.LOW)
            end
         end
         gpio.write(en, gpio.LOW)
         return rv
      end
      if read then setGPIO(gpio.INPUT) end
      value = exchange(value)
      if read then setGPIO(gpio.OUTPUT) end
      return value
   end

   -- Return backend object
   return {
      fourbits  = false,
      command   = function (_, cmd)
         return send8bitGPIO(cmd, false, false, false)
      end,
      busy      = function(_)
         if rw == nil then return false end
         return bit.isset(send8bitGPIO(0xff, false, true, true), 7)
      end,
      position  = function(_)
         if rw == nil then return 0 end
         return bit.clear(send8bitGPIO(0xff, false, true, true), 7)
      end,
      write     = function(_, value)
         return send8bitGPIO(value, true, false, false)
      end,
      read      = function(_)
         if rw == nil then return nil end
         return send8bitGPIO(0xff, true, true, true)
      end,
      backlight = function(_, on)
         if (bl) then gpio.write(bl, on and gpio.HIGH or gpio.LOW) end
         return on
      end,
   }

end
