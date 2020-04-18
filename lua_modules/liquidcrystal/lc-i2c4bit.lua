local i2c, bit = i2c, bit

return function(bus_args)
   local busid = bus_args.id or 0
   local busad = bus_args.address or 0x27
   local speed = bus_args.speed or i2c.SLOW

   local rs = bus_args.rs or 0
   local rw = bus_args.rw or 1
   local en = bus_args.en or 2
   local bl = bus_args.backlight or 3
   local d4 = bus_args.d4 or 4
   local d5 = bus_args.d5 or 5
   local d6 = bus_args.d6 or 6
   local d7 = bus_args.d7 or 7

   -- Convenience I2C setup if a pin configuration is given
   if bus_args.sda ~= nil and bus_args.scl ~= nil then
      i2c.setup(busid, bus_args.sda, bus_args.scl, speed)
   end

   -- The onus is on us to maintain the backlight state
   local backlight = true

   local function send4bitI2C(value, rs_en, rw_en, read)
      local function exchange(data, unset_read)
         local rv = data
         i2c.start(busid)
         i2c.address(busid, busad, i2c.TRANSMITTER)
         i2c.write(busid, bit.set(data, en))
         if read then
            i2c.start(busid)
            i2c.address(busid, busad, i2c.RECEIVER)
            rv = i2c.read(busid, 1):byte(1)
            i2c.start(busid)
            i2c.address(busid, busad, i2c.TRANSMITTER)
            if unset_read then data = bit.bor(bit.bit(rs),
                                              bit.bit(rw),
                                              backlight and bit.bit(bl) or 0) end
	    i2c.write(busid, bit.set(data, en))
         end
         i2c.write(busid, bit.clear(data, en))
         i2c.stop(busid)
         return rv
      end
      local lo = bit.bor(bit.isset(value, 0) and bit.bit(d4) or 0,
                         bit.isset(value, 1) and bit.bit(d5) or 0,
                         bit.isset(value, 2) and bit.bit(d6) or 0,
                         bit.isset(value, 3) and bit.bit(d7) or 0)
      local hi = bit.bor(bit.isset(value, 4) and bit.bit(d4) or 0,
                         bit.isset(value, 5) and bit.bit(d5) or 0,
                         bit.isset(value, 6) and bit.bit(d6) or 0,
                         bit.isset(value, 7) and bit.bit(d7) or 0)
      local cmd = bit.bor(rs_en and bit.bit(rs) or 0,
                          rw_en and bit.bit(rw) or 0,
                          backlight and bit.bit(bl) or 0)
      hi = exchange(bit.bor(cmd, hi), false)
      lo = exchange(bit.bor(cmd, lo), true)
      return bit.bor(bit.lshift(bit.isset(lo, d4) and 1 or 0, 0),
                     bit.lshift(bit.isset(lo, d5) and 1 or 0, 1),
                     bit.lshift(bit.isset(lo, d6) and 1 or 0, 2),
                     bit.lshift(bit.isset(lo, d7) and 1 or 0, 3),
                     bit.lshift(bit.isset(hi, d4) and 1 or 0, 4),
                     bit.lshift(bit.isset(hi, d5) and 1 or 0, 5),
                     bit.lshift(bit.isset(hi, d6) and 1 or 0, 6),
                     bit.lshift(bit.isset(hi, d7) and 1 or 0, 7))
   end

   -- init sequence from datasheet
   send4bitI2C(0x33, false, false, false)
   send4bitI2C(0x32, false, false, false)

   -- Return backend object
   return {
      fourbits  = true,
      command   = function (screen, cmd)
         return send4bitI2C(cmd, false, false, false)
      end,
      busy      = function(screen)
         local rv = send4bitI2C(0xff, false, true, true)
         send4bitI2C(bit.bor(0x80, bit.clear(rv, 7)), false, false, false)
         return bit.isset(rv, 7)
      end,
      position  = function(screen)
         local rv = bit.clear(send4bitI2C(0xff, false, true, true), 7)
         send4bitI2C(bit.bor(0x80, rv), false, false, false)
         return rv
      end,
      write     = function(screen, value)
         return send4bitI2C(value, true, false, false)
      end,
      read      = function(screen)
         return send4bitI2C(0xff, true, true, true)
      end,
      backlight = function(screen, on)
         backlight = on
         local rv = bit.clear(send4bitI2C(0xff, false, true, true), 7)
         send4bitI2C(bit.bor(0x80, rv), false, false, false)
         return on
      end,
   }

end
