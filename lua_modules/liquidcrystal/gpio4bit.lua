local GPIO4bit = {}

local function send4bitGPIO(value, cmd, bus_args)
   if cmd then
      gpio.write(bus_args.rs, gpio.LOW)
   else
      gpio.write(bus_args.rs, gpio.HIGH)
   end
   local hi = bit.rshift(bit.band(value, 0xf0), 4)
   local lo = bit.band(value, 0xf)
   for i=0,3 do
      if bit.band(bit.lshift(1, i), hi) ~= 0 then
	 gpio.write(bus_args[i], gpio.HIGH)
      else
	 gpio.write(bus_args[i], gpio.LOW)
      end
   end
   gpio.write(bus_args.en, gpio.HIGH)
   gpio.write(bus_args.en, gpio.LOW)
   for i=0,3 do
      if bit.band(bit.lshift(1, i), lo) ~= 0 then
	 gpio.write(bus_args[i], gpio.HIGH)
      else
	 gpio.write(bus_args[i], gpio.LOW)
      end
   end
   gpio.write(bus_args.en, gpio.HIGH)
   gpio.write(bus_args.en, gpio.LOW)
end

GPIO4bit.command = function(screen, value)
   send4bitGPIO(value, true, screen._bus_args)
end

GPIO4bit.write = function(screen, value)
   send4bitGPIO(value, false, screen._bus_args)
end

GPIO4bit.backlight = function(screen, on)
   if screen._bus_args.bl ~= nil then
      if on then
	 gpio.write(screen._bus_args.bl, gpio.HIGH)
      else
	 gpio.write(screen._bus_args.bl, gpio.LOW)
      end
   end
end

GPIO4bit.init = function(screen)
   for _,i in pairs(screen._bus_args) do
      gpio.mode(i, gpio.OUTPUT)
   end
   -- init sequence from datasheet
   screen._backend.command(screen, 0x33)
   screen._backend.command(screen, 0x32)
end

return GPIO4bit
