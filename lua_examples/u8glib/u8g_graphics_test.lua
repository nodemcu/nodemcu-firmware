-- ***************************************************************************
-- Graphics Test
--
-- This script executes several features of u8glib to test their Lua bindings.
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


-- graphic test components
function prepare()
    disp:setFont(u8g.font_6x10)
    disp:setFontRefHeightExtendedText()
    disp:setDefaultForegroundColor()
    disp:setFontPosTop()
end

function box_frame(a)
    disp:drawStr(0, 0, "drawBox")
    disp:drawBox(5, 10, 20, 10)
    disp:drawBox(10+a, 15, 30, 7)
    disp:drawStr(0, 30, "drawFrame")
    disp:drawFrame(5, 10+30, 20, 10)
    disp:drawFrame(10+a, 15+30, 30, 7)
end

function disc_circle(a)
    disp:drawStr(0, 0, "drawDisc")
    disp:drawDisc(10, 18, 9)
    disp:drawDisc(24+a, 16, 7)
    disp:drawStr(0, 30, "drawCircle")
    disp:drawCircle(10, 18+30, 9)
    disp:drawCircle(24+a, 16+30, 7)
end

function r_frame(a)
    disp:drawStr(0, 0, "drawRFrame/Box")
    disp:drawRFrame(5, 10, 40, 30, a+1)
    disp:drawRBox(50, 10, 25, 40, a+1)
end

function stringtest(a)
    disp:drawStr(30+a, 31, " 0")
    disp:drawStr90(30, 31+a, " 90")
    disp:drawStr180(30-a, 31, " 180")
    disp:drawStr270(30, 31-a, " 270")
end

function line(a)
    disp:drawStr(0, 0, "drawLine")
    disp:drawLine(7+a, 10, 40, 55)
    disp:drawLine(7+a*2, 10, 60, 55)
    disp:drawLine(7+a*3, 10, 80, 55)
    disp:drawLine(7+a*4, 10, 100, 55)
end

function triangle(a)
    local offset = a
    disp:drawStr(0, 0, "drawTriangle")
    disp:drawTriangle(14,7, 45,30, 10,40)
    disp:drawTriangle(14+offset,7-offset, 45+offset,30-offset, 57+offset,10-offset)
    disp:drawTriangle(57+offset*2,10, 45+offset*2,30, 86+offset*2,53)
    disp:drawTriangle(10+offset,40+offset, 45+offset,30+offset, 86+offset,53+offset)
end

function ascii_1()
    local x, y, s
    disp:drawStr(0, 0, "ASCII page 1")
    for y = 0, 5, 1 do
        for x = 0, 15, 1 do
            s = y*16 + x + 32
            disp:drawStr(x*7, y*10+10, string.char(s))
        end
    end
end

function extra_page(a)
    disp:drawStr(0, 12, "setScale2x2")
    disp:setScale2x2()
    disp:drawStr(0, 6+a, "setScale2x2")
    disp:undoScale()
end


-- the draw() routine
function draw(draw_state)
    local component = bit.rshift(draw_state, 3)

    prepare()

    if (component == 0) then
        box_frame(bit.band(draw_state, 7))
    elseif (component == 1) then
        disc_circle(bit.band(draw_state, 7))
    elseif (component == 2) then
        r_frame(bit.band(draw_state, 7))
    elseif (component == 3) then
        stringtest(bit.band(draw_state, 7))
    elseif (component == 4) then
        line(bit.band(draw_state, 7))
    elseif (component == 5) then
        triangle(bit.band(draw_state, 7))
    elseif (component == 6) then
        ascii_1()
    elseif (component == 7) then
        extra_page(bit.band(draw_state, 7))
    end
end

function draw_loop()
    -- Draws one page and schedules the next page, if there is one
    local function draw_pages()
        draw(draw_state)
        if disp:nextPage() then
            node.task.post(draw_pages)
        else
            node.task.post(graphics_test)
        end
    end
    -- Restart the draw loop and start drawing pages
    disp:firstPage()
    node.task.post(draw_pages)
end

function graphics_test()

    if (draw_state <= 7 + 8*8) then
        draw_state = draw_state + 1
    else
        print("--- Restarting Graphics Test ---")
        draw_state = 0
    end

    print("Heap: " .. node.heap())
    -- retrigger draw_loop
    node.task.post(draw_loop)

end

draw_state = 0

init_i2c_display()
--init_spi_display()

print("--- Starting Graphics Test ---")
node.task.post(draw_loop)
