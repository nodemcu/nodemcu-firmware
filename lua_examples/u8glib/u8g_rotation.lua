-- ***************************************************************************
-- Rotation Test
--
-- This script executes the rotation features of u8glib to test their Lua
-- integration.
--
-- Note: It is prepared for SSD1306-based displays. Select your connectivity
--       type by calling either init_i2c_display() or init_spi_display() at
--       the bottom of this file.
--
-- ***************************************************************************

-- setup I2c and connect display
function init_i2c_display()
     -- SDA and SCL can be assigned freely to available GPIOs
     local sda = 5 -- GPIO14
     local scl = 6 -- GPIO12
     local sla = 0x3c
     i2c.setup(0, sda, scl, i2c.SLOW)
     disp = u8g.ssd1306_128x64_i2c(sla)
end

-- setup SPI and connect display
function init_spi_display()
     -- Hardware SPI CLK  = GPIO14
     -- Hardware SPI MOSI = GPIO13
     -- Hardware SPI MISO = GPIO12 (not used)
     -- Hardware SPI /CS  = GPIO15 (not used)
     -- CS, D/C, and RES can be assigned freely to available GPIOs
     local cs  = 8 -- GPIO15, pull-down 10k to GND
     local dc  = 4 -- GPIO2
     local res = 0 -- GPIO16

     spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)
     -- we won't be using the HSPI /CS line, so disable it again
     gpio.mode(8, gpio.INPUT, gpio.PULLUP)

     disp = u8g.ssd1306_128x64_hw_spi(cs, dc, res)
end


-- the draw() routine
function draw()
     disp:setFont(u8g.font_6x10)
     disp:drawStr( 0+0, 20+0, "Hello!")
     disp:drawStr( 0+2, 20+16, "Hello!")

     disp:drawBox(0, 0, 3, 3)
     disp:drawBox(disp:getWidth()-6, 0, 6, 6)
     disp:drawBox(disp:getWidth()-9, disp:getHeight()-9, 9, 9)
     disp:drawBox(0, disp:getHeight()-12, 12, 12)
end


function rotate()
     if (next_rotation < tmr.now() / 1000) then
          if (dir == 0) then
               disp:undoRotation()
          elseif (dir == 1) then
               disp:setRot90()
          elseif (dir == 2) then
               disp:setRot180()
          elseif (dir == 3) then
               disp:setRot270()
          end

          dir = dir + 1
          dir = bit.band(dir, 3)
          -- schedule next rotation step in 1000ms
          next_rotation = tmr.now() / 1000 + 1000
     end
end

function rotation_test()
     print("--- Starting Rotation Test ---")
     dir = 0
     next_rotation = 0

     local loopcnt
     for loopcnt = 1, 100, 1 do
          rotate()

          disp:firstPage()
          repeat
               draw(draw_state)
          until disp:nextPage() == false

          tmr.delay(100000)
          tmr.wdclr()
     end

     print("--- Rotation Test done ---")
end

--init_i2c_display()
init_spi_display()
rotation_test()
