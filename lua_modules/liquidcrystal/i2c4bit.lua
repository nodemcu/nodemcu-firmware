local I2C4bit = {}

local function send4bitI2C(value, cmd, bus_args, backlight)
   local hi = bit.bor(bit.lshift(bit.band(bit.rshift(value, 4), 1), bus_args.d4),
		      bit.lshift(bit.band(bit.rshift(value, 5), 1), bus_args.d5),
		      bit.lshift(bit.band(bit.rshift(value, 6), 1), bus_args.d6),
		      bit.lshift(bit.band(bit.rshift(value, 7), 1), bus_args.d7))
   local lo = bit.bor(bit.lshift(bit.band(bit.rshift(value, 0), 1), bus_args.d4),
		      bit.lshift(bit.band(bit.rshift(value, 1), 1), bus_args.d5),
		      bit.lshift(bit.band(bit.rshift(value, 2), 1), bus_args.d6),
		      bit.lshift(bit.band(bit.rshift(value, 3), 1), bus_args.d7))
   if backlight then
      cmd = bit.bor(cmd, bit.lshift(1, bus_args.backlight))
   end
   i2c.start(bus_args.id)
   i2c.address(bus_args.id, bus_args.address, i2c.TRANSMITTER)
   i2c.write(bus_args.id, bit.bor(hi, cmd, bit.lshift(1, bus_args.en)))
   i2c.write(bus_args.id, bit.bor(hi, bit.band(cmd, bit.bnot(bit.lshift(1, bus_args.en)))))
   i2c.write(bus_args.id, bit.bor(lo, cmd, bit.lshift(1, bus_args.en)))
   i2c.write(bus_args.id, bit.bor(lo, bit.band(cmd, bit.bnot(bit.lshift(1, bus_args.en)))))
   i2c.stop(bus_args.id)
end

I2C4bit.command = function(screen, value)
   send4bitI2C(value, 0x0, screen._bus_args, screen._backlight)
end

I2C4bit.write = function(screen, value)
   send4bitI2C(value, bit.lshift(1, screen._bus_args.rs), screen._bus_args, screen._backlight)
end

I2C4bit.backlight = function(screen, on)
   screen._backlight = on
   screen._backend.command(screen, 0x0)
end

I2C4bit.init = function(screen)
   screen._bus_args.rs = screen._bus_args.rs or 0
   screen._bus_args.rw = screen._bus_args.rw or 1
   screen._bus_args.en = screen._bus_args.en or 2
   screen._bus_args.backlight = screen._bus_args.backlight or 3
   screen._bus_args.d4 = screen._bus_args.d4 or 4
   screen._bus_args.d5 = screen._bus_args.d5 or 5
   screen._bus_args.d6 = screen._bus_args.d6 or 6
   screen._bus_args.d7 = screen._bus_args.d7 or 7
   if screen._bus_args.sda ~= nil then
      i2c.setup(screen._bus_args.id, screen._bus_args.sda, screen._bus_args.scl, screen._bus_args.speed)
   end
   screen._backlight = true
   -- init sequence from datasheet
   screen._backend.command(screen, 0x33)
   screen._backend.command(screen, 0x32)
end

return I2C4bit
