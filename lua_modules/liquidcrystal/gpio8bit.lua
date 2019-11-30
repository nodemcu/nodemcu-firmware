local GPIO8bit = {}

local function send8bitGPIO(value, cmd, bus_args)
   if cmd then
      gpio.write(bus_args.rs, gpio.LOW)
   else
      gpio.write(bus_args.rs, gpio.HIGH)
   end
   for i=0,7 do
      if bit.band(bit.lshift(1, i), value) ~= 0 then
	 gpio.write(bus_args[i], gpio.HIGH)
      else
	 gpio.write(bus_args[i], gpio.LOW)
      end
   end
   gpio.write(bus_args.en, gpio.HIGH)
   gpio.write(bus_args.en, gpio.LOW)
end

GPIO8bit.command = function(screen, value)
   send8bitGPIO(value, true, screen._bus_args)
end

GPIO8bit.write = function(screen, value)
   send8bitGPIO(value, false, screen._bus_args)
end

GPIO8bit.backlight = function(screen, on)
   if screen._bus_args.bl ~= nil then
      if on then
	 gpio.write(screen._bus_args.bl, gpio.HIGH)
      else
	 gpio.write(screen._bus_args.bl, gpio.LOW)
      end
   end
end

GPIO8bit.init = function(screen)
   for _,i in pairs(screen._bus_args) do
      gpio.mode(i, gpio.OUTPUT)
   end
end

return GPIO8bit
