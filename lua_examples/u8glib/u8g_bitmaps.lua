-- ***************************************************************************
-- Bitmaps Test
--
-- This script executes the bitmap features of u8glib to test their Lua
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


function xbm_picture()
     disp:setFont(u8g.font_6x10)
     disp:drawStr( 0, 10, "XBM picture")

     disp:drawXBM( 0, 20, 38, 24, xbm_data )
end

function bitmap_picture(state)
     disp:setFont(u8g.font_6x10)
     disp:drawStr( 0, 10, "Bitmap picture")

     disp:drawBitmap( 0 + (state * 10), 20 + (state * 4), 1, 8, bm_data )
end

-- the draw() routine
function draw(draw_state)
     local component = bit.rshift(draw_state, 3)

     if (component == 0) then
          xbm_picture(bit.band(draw_state, 7))
     elseif (component == 1) then
          bitmap_picture(bit.band(draw_state, 7))
     end
end


function draw_loop()
    -- Draws one page and schedules the next page, if there is one
    local function draw_pages()
        draw(draw_state)
        if disp:nextPage() then
            node.task.post(draw_pages)
        else
            node.task.post(bitmap_test)
        end
    end
    -- Restart the draw loop and start drawing pages
    disp:firstPage()
    node.task.post(draw_pages)
end

function bitmap_test()

    if (draw_state <= 7 + 1*8) then
        draw_state = draw_state + 1
    else
        print("--- Restarting Bitmap Test ---")
        draw_state = 1
    end

    print("Heap: " .. node.heap())
    -- retrigger draw_loop
    node.task.post(draw_loop)

end

draw_state = 1

init_i2c_display()
--init_spi_display()

-- read XBM picture
file.open("u8glib_logo.xbm", "r")
xbm_data = file.read()
file.close()

-- read Bitmap picture
file.open("u8g_rook.bm", "r")
bm_data = file.read()
file.close()


print("--- Starting Bitmap Test ---")
node.task.post(draw_loop)
