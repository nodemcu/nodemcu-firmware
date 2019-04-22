-- ChiliPeppr Joystick controller
-- Used to control analog joysticks that are implemented just using
-- variable resistors where you feed in 3.3v and get out a variable voltage
-- based on position of the joystick. Joysticks like the Playstation portable
-- or Xbox joysticks work this way and are readily available on Amazon, Ebay,
-- or Aliexpress.
--
-- Working example of using this library:
-- https://www.youtube.com/watch?v=ITgh5epyPRk
--
-- Visit http://chilipeppr.com/esp32
-- By John Lauer
--
-- There is no license needed to use/modify this software. It is given freely
-- to the open source community. Modify at will.
-- 
joystick = require("joystick")

joystick.on("xy", function(x, y)
  print("Got xy change. x:", x, "y:", y)
end)

-- joystick.on("x", function(x)
--   print("Got x change. x:", x)
-- end)

-- joystick.on("y", function(y)
--   print("Got y change. y:", y)
-- end)

joystick.on("xcenter", function()
  print("Got x center")
end)

joystick.on("ycenter", function()
  print("Got y center")
end)

joystick.on("button", function(val)
  print("Got button:", val)
end)

joystick.init()

