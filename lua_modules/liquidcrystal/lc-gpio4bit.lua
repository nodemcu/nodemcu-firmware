local gpio, bit = gpio, bit

return function(bus_args)
   local rs = bus_args.rs or 0
   local rw = bus_args.rw
   local en = bus_args.en or 1
   local bl = bus_args.backlight
   local d4 = bus_args.d4 or 2
   local d5 = bus_args.d5 or 3
   local d6 = bus_args.d6 or 4
   local d7 = bus_args.d7 or 5

   for _, d in pairs({rs,rw,en,bl}) do
      if d then
         gpio.mode(d, gpio.OUTPUT)
      end
   end

   local function setGPIO(mode)
      for _, d in pairs({d4, d5, d6, d7}) do
         gpio.mode(d, mode)
      end
   end

   setGPIO(gpio.OUTPUT)

   local function send4bitGPIO(value, rs_en, rw_en, read)
      local function exchange(data)
         local rv = 0
         if rs then gpio.write(rs, rs_en and gpio.HIGH or gpio.LOW) end
         if rw then gpio.write(rw, rw_en and gpio.HIGH or gpio.LOW) end
         gpio.write(en, gpio.HIGH)
         for i, d in ipairs({d4, d5, d6, d7}) do
            if read and rw then
               if gpio.read(d) == 1 then rv = bit.set(rv, i-1) end
            else
               gpio.write(d, bit.isset(data, i-1) and gpio.HIGH or gpio.LOW)
            end
         end
         gpio.write(en, gpio.LOW)
         return rv
      end
      local hi = bit.rshift(bit.band(value, 0xf0), 4)
      local lo = bit.band(value, 0xf)
      if read then setGPIO(gpio.INPUT) end
      hi = exchange(hi)
      lo = exchange(lo)
      if read then setGPIO(gpio.OUTPUT) end
      return bit.bor(bit.lshift(hi, 4), lo)
   end

   -- init sequence from datasheet
   send4bitGPIO(0x33, false, false, false)
   send4bitGPIO(0x32, false, false, false)

   -- Return backend object
   return {
      fourbits  = true,
      command   = function (_, cmd)
         return send4bitGPIO(cmd, false, false, false)
      end,
      busy      = function(_)
         if rw == nil then return false end
         return bit.isset(send4bitGPIO(0xff, false, true, true), 7)
      end,
      position  = function(_)
         if rw == nil then return 0 end
         return bit.clear(send4bitGPIO(0xff, false, true, true), 7)
      end,
      write     = function(_, value)
         return send4bitGPIO(value, true, false, false)
      end,
      read      = function(_)
         if rw == nil then return nil end
         return send4bitGPIO(0xff, true, true, true)
      end,
      backlight = function(_, on)
         if (bl) then gpio.write(bl, on and gpio.HIGH or gpio.LOW) end
         return on
      end,
   }

end
