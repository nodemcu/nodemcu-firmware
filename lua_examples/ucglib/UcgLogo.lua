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

    -- initialize the matching driver for your display
    -- see app/include/ucg_config.h
    --disp = ucg.ili9341_18x240x320_hw_spi(cs, dc, res)
    disp = ucg.st7735_18x128x160_hw_spi(cs, dc, res)
end


function upper_pin(x, y)
    local w = 7
    local h = 6
    disp:setColor(0, 212, 212, 212)
    disp:setColor(1, 200, 200, 200)
    disp:setColor(2, 200, 200, 200)
    disp:setColor(3, 188, 188, 188)
    disp:drawGradientBox(x, y, w, h)

    --disp:drawVLine(x+w, y+1, len)
    disp:setColor(0, 220, 220, 220)
    disp:setColor(1, 232, 232, 232)
    disp:drawGradientLine(x+w, y, h, 1)
end

function lower_pin(x, y)
    local w = 7
    local h = 5
    disp:setColor(0, 212, 212, 212)
    disp:setColor(1, 200, 200, 200)
    disp:setColor(2, 200, 200, 200)
    disp:setColor(3, 188, 188, 188)
    disp:drawGradientBox(x, y, w, h)

    --disp:drawVLine(x+w, y+1, len)
    disp:setColor(0, 220, 220, 220)
    disp:setColor(1, 232, 232, 232)
    disp:drawGradientLine(x+w, y, h, 1)
    disp:setColor(0, 220, 220, 220)
    disp:setColor(1, 232, 232, 232)
    disp:drawGradientLine(x, y+h, w, 0)
    disp:setColor(0, 240, 240, 240)
    disp:drawPixel(x+w, y+h)
end

function ic_body(x, y)
    local w = 4*14+4
    local h = 31
    disp:setColor(0, 60, 60, 60)
    disp:setColor(1, 40, 40, 40)
    disp:setColor(2, 48, 48, 48)
    disp:setColor(3, 30, 30, 30)
    disp:drawGradientBox(x, y, w, h)
  
    disp:setColor(0, 255, 168, 0)
    --disp:setColor(0, 225, 168, 30)
    disp:drawDisc(x+w-1, y+h/2-1, 7, bit.bor(ucg.DRAW_UPPER_LEFT, ucg.DRAW_LOWER_LEFT))

    disp:setColor(0, 60, 30, 0)
    --disp:drawDisc(x+w-1, y+h/2+1, 7, bit.bor(ucg.DRAW_UPPER_LEFT, ucg.DRAW_LOWER_LEFT))

    disp:setColor(0, 50, 50, 50)
    disp:setColor(0, 25, 25, 25)
    disp:drawDisc(x+w-1, y+h/2+1, 7, bit.bor(ucg.DRAW_UPPER_LEFT, ucg.DRAW_LOWER_LEFT))
end

function draw_ucg_logo()
    local a, b
  
    --ucg_Init(ucg, ucg_sdl_dev_cb, ucg_ext_none, (ucg_com_fnptr)0)
    disp:setFont(ucg.font_ncenB24_tr)

    --disp:setRotate270()
    --disp:setClipRange(10,5,40,20)
  
    a = 2
    b = 3
  
    disp:setColor(0, 135*a/b,206*a/b,250*a/b)
    disp:setColor(1, 176*a/b,226*a/b,255*a/b)
    disp:setColor(2, 25*a/b,25*a/b,112*a/b)
    disp:setColor(3, 85*a/b,26*a/b,139*a/b)
    disp:drawGradientBox(0, 0, disp:getWidth()/4, disp:getHeight())

    disp:setColor(1, 135*a/b,206*a/b,250*a/b)
    disp:setColor(0, 176*a/b,226*a/b,255*a/b)
    disp:setColor(3, 25*a/b,25*a/b,112*a/b)
    disp:setColor(2, 85*a/b,26*a/b,139*a/b)
    disp:drawGradientBox(disp:getWidth()/4, 0, disp:getWidth()/4, disp:getHeight())

    disp:setColor(0, 135*a/b,206*a/b,250*a/b)
    disp:setColor(1, 176*a/b,226*a/b,255*a/b)
    disp:setColor(2, 25*a/b,25*a/b,112*a/b)
    disp:setColor(3, 85*a/b,26*a/b,139*a/b)
    disp:drawGradientBox(disp:getWidth()*2/4, 0, disp:getWidth()/4, disp:getHeight())

    disp:setColor(1, 135*a/b,206*a/b,250*a/b)
    disp:setColor(0, 176*a/b,226*a/b,255*a/b)
    disp:setColor(3, 25*a/b,25*a/b,112*a/b)
    disp:setColor(2, 85*a/b,26*a/b,139*a/b)
    disp:drawGradientBox(disp:getWidth()*3/4, 0, disp:getWidth()/4, disp:getHeight())


    upper_pin(7+0*14, 4)
    upper_pin(7+1*14, 4)
    upper_pin(7+2*14, 4)
    upper_pin(7+3*14, 4)
  
    ic_body(2, 10)

    lower_pin(7+0*14, 41)
    lower_pin(7+1*14, 41)
    lower_pin(7+2*14, 41)
    lower_pin(7+3*14, 41)

    disp:setColor(0, 135*a/b, 206*a/b, 250*a/b)
    disp:drawString(63+1, 33+1, 0, "glib")

    disp:setColor(0, 255, 168, 0)
    disp:drawGlyph(26, 38, 0, 'U')
    disp:drawString(63, 33, 0, "glib")

    disp:setColor(0, 135*a/b, 206*a/b, 250*a/b)
    disp:setColor(1, 135*a/b, 206*a/b, 250*a/b)
    disp:setColor(2, 135*a/b, 206*a/b, 250*a/b)
    disp:setColor(3, 135*a/b, 206*a/b, 250*a/b)
    disp:drawGradientBox(84+1, 42+1-6, 42, 4)

    disp:setColor(0, 255, 180, 40)
    disp:setColor(1, 235, 148, 0)
    --disp:drawGradientLine(79, 42, 20, 0)
    disp:setColor(2, 245, 158, 0)
    disp:setColor(3, 220, 138, 0)
    disp:drawGradientBox(84, 42-6, 42, 4)

    disp:setColor(0, 255, 168, 0)
    --disp:setFont(ucg.font_5x8_tr)
    disp:setFont(ucg.font_7x13B_tr)
    --disp:setFont(ucg.font_courB08_tr)
    --disp:setFont(ucg.font_timR08_tr)
    disp:drawString(2, 54+5, 0, "http://github.com")
    disp:drawString(2, 61+10, 0, "/olikraus/ucglib")
    --disp:drawString(1, 61, 0, "code.google.com/p/ucglib/")
end


init_spi_display()

disp:begin(ucg.FONT_MODE_TRANSPARENT)
disp:clearScreen()


disp:setRotate180()
draw_ucg_logo()
