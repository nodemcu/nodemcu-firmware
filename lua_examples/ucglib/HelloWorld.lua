-- setup SPI and connect display
function init_spi_display()
  -- pins can be assigned freely to available GPIOs
  local sclk = 19
  local mosi = 23
  local cs   = 22
  local dc   = 16
  local res  = 17

  local bus = spi.master(spi.HSPI, {sclk=sclk, mosi=mosi})

  --disp = ucg.ili9341_18x240x320_hw_spi(bus, cs, dc, res)
  disp = ucg.st7735_18x128x160_hw_spi(bus, cs, dc, res)
end



init_spi_display()

disp:begin(ucg.FONT_MODE_TRANSPARENT)
disp:clearScreen()

disp:setFont(ucg.font_ncenR12_tr);
disp:setColor(255, 255, 255);
disp:setColor(1, 255, 0,0);


disp:setPrintPos(0, 25)
disp:print("Hello World!")
