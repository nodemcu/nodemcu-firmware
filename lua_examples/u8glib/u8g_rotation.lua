
-- setup I2c and connect display
function init_i2c_display()
     -- SDA and SCL can be assigned freely to available GPIOs
     sda = 5 -- GPIO14
     scl = 6 -- GPIO12
     sla = 0x3c
     i2c.setup(0, sda, scl, i2c.SLOW)
     disp = u8g.ssd1306_128x64_i2c(sla)
end

-- setup SPI and connect display
function init_spi_display()
     -- Hardware SPI CLK  = GPIO14
     -- Hardware SPI MOSI = GPIO13
     -- Hardware SPI MISO = GPIO12 (not used)
     -- CS, D/C, and RES can be assigned freely to available GPIOs
     cs  = 8 -- GPIO15, pull-down 10k to GND
     dc  = 4 -- GPIO2
     res = 0 -- GPIO16

     spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, spi.DATABITS_8, 0)
     disp = u8g.ssd1306_128x64_spi(cs, dc, res)
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
