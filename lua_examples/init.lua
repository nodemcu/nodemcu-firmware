-- init.lua example, with wifi portal
--
-- Waits 10 sec to allow stopping the boot process, and get wifi activated
-- When wifi not activated, setup a portal to select a network
-- In all cases, wifi ends up activated and the main task main.lua is executed
--
-- module needed: enduser_setup

print("Waiting 10sec... use t:stop() to cancel init.lua")
t=tmr.create()
t:alarm(10000,tmr.ALARM_SINGLE,function ()
	t=nil -- free the timer
	if wifi.sta.getip() then
		print("Wifi activated",wifi.sta.getip())
		dofile("main.lua")
	else
		print("No Wifi. Starting portal...")
		enduser_setup.start(
		  "SuperBidule",
		  function()
			wifi.sta.autoconnect(1)
			print("Wifi activated",wifi.sta.getip())
			dofile("main.lua")
		  end,
		  function(err, str)
		    print("enduser_setup: Err #" .. err .. ": " .. str)
		  end,
		  function(str)
		    print("Debug " .. str)
		  end
		)
	end
end)

