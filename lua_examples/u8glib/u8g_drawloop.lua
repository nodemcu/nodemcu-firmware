------------------------------------------------------------------------------
-- u8glib example which shows how to implement the draw loop without causing
-- timeout issues with the WiFi stack. This is done by drawing one page at
-- a time, allowing the ESP SDK to do it's house keeping between the page
-- draws.
--
-- This example assumes you have an SSD1306 display connected to pins 4 and 5
-- using I2C and that the profont22r is compiled into the firmware.
-- Please edit the init_display function to match your setup.
--  
-- Example:
-- dofile("u8g_displayloop.lua")
------------------------------------------------------------------------------

local updateDrawFunc
local disp
local font

function init_display()
  local sda = 4
  local sdl = 5
  local sla = 0x3c
  i2c.setup(0,sda,sdl, i2c.SLOW)
  disp = u8g.ssd1306_128x64_i2c(sla)
  font = u8g.font_profont22r
end

local function setLargeFont()
  disp:setFont(font)
  disp:setFontRefHeightExtendedText()
  disp:setDefaultForegroundColor()
  disp:setFontPosTop()
end

-- Draws one page and schedules the next page, if there is one
local function drawPages()
  updateDrawFunc()
  if (disp:nextPage() == true) then
    node.task.post(drawPages)
  end
end  

-- Start the draw loop with the draw implementation in the provided function callback
function updateDisplay(func)
  updateDrawFunc = func
  disp:firstPage()
  node.task.post(drawPages)
end

function drawHello()
  setLargeFont()
  disp:drawStr(30,22, "Hello")
end

function drawWorld()
  setLargeFont()
  disp:drawStr(30,22, "World")
end

local drawDemo = { drawHello, drawWorld }
local drawIndex = 1

function demoLoop()
  -- Start the draw loop with one of the demo functions
  updateDisplay(drawDemo[drawIndex])
  drawIndex = drawIndex + 1
  if drawDemo[drawIndex] == nil then
    drawIndex = 1
  end  
end

-- Initialise the display and start the demo loop
init_display()
demoLoop()
tmr.alarm(4, 5000, 1, demoLoop)
