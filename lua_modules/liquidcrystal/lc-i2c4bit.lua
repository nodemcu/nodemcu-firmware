local i2c, bit = i2c, bit --luacheck: read globals i2c bit

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

   local function exchange(data, read)
      local rv = data
      i2c.start(busid)
      i2c.address(busid, busad, i2c.TRANSMITTER)
      i2c.write(busid, bit.set(data, en)) -- set data with en
      if read then
         i2c.start(busid) -- read 1 byte and go back to tx mode
         i2c.address(busid, busad, i2c.RECEIVER)
         rv = i2c.read(busid, 1):byte(1)
         i2c.start(busid)
         i2c.address(busid, busad, i2c.TRANSMITTER)
      end
      i2c.write(busid, data) -- lower en
      i2c.stop(busid)
      return rv
   end

   local function send4bitI2C(value, rs_en, rw_en)
      local meta = bit.bor(rs_en and bit.bit(rs) or 0,
                          rw_en and bit.bit(rw) or 0,
                          backlight and bit.bit(bl) or 0)
      local lo = bit.bor(bit.isset(value, 0) and bit.bit(d4) or 0,
                         bit.isset(value, 1) and bit.bit(d5) or 0,
                         bit.isset(value, 2) and bit.bit(d6) or 0,
                         bit.isset(value, 3) and bit.bit(d7) or 0)
      local hi = bit.bor(bit.isset(value, 4) and bit.bit(d4) or 0,
                         bit.isset(value, 5) and bit.bit(d5) or 0,
                         bit.isset(value, 6) and bit.bit(d6) or 0,
                         bit.isset(value, 7) and bit.bit(d7) or 0)
      hi = exchange(bit.bor(meta, hi), rw_en)
      lo = exchange(bit.bor(meta, lo), rw_en)
      return bit.bor(bit.lshift(bit.isset(lo, d4) and 1 or 0, 0),
                     bit.lshift(bit.isset(lo, d5) and 1 or 0, 1),
                     bit.lshift(bit.isset(lo, d6) and 1 or 0, 2),
                     bit.lshift(bit.isset(lo, d7) and 1 or 0, 3),
                     bit.lshift(bit.isset(hi, d4) and 1 or 0, 4),
                     bit.lshift(bit.isset(hi, d5) and 1 or 0, 5),
                     bit.lshift(bit.isset(hi, d6) and 1 or 0, 6),
                     bit.lshift(bit.isset(hi, d7) and 1 or 0, 7))
   end

   -- init sequence from datasheet (Figure 24)
   local function justsend(what)
     i2c.start(busid)
     i2c.address(busid, busad, i2c.TRANSMITTER)
     i2c.write(busid, bit.set(what, en))
     i2c.write(busid, what)
     i2c.stop(busid)
   end
   local three = bit.bor(bit.bit(d4), bit.bit(d5))
   justsend(three)
   tmr.delay(5)
   justsend(three)
   tmr.delay(1)
   justsend(three)
   tmr.delay(1)
   justsend(bit.bit(d5))
   -- we are now primed for the FUNCTIONSET command from the liquidcrystal ctor

   -- Return backend object
   return {
      fourbits  = true,
      command   = function (_, cmd)
         return send4bitI2C(cmd, false, false)
      end,
      busy      = function(_)
         return bit.isset(send4bitI2C(0xff, false, true), 7)
      end,
      position  = function(_)
         return bit.clear(send4bitI2C(0xff, false, true), 7)
      end,
      write     = function(_, value)
         return send4bitI2C(value, true, false)
      end,
      read      = function(_)
         return send4bitI2C(0xff, true, true)
      end,
      backlight = function(_, on)
         backlight = on
         send4bitI2C(0, false, false) -- No-op
         return on
      end,
   }

end
