-- setup SPI and connect display
function init_spi_display()
    -- Hardware SPI CLK  = GPIO14
    -- Hardware SPI MOSI = GPIO13
    -- Hardware SPI MISO = GPIO12 (not used)
    -- CS, D/C, and RES can be assigned freely to available GPIOs
    local cs  = 8 -- GPIO15, pull-down 10k to GND
    local dc  = 4 -- GPIO2
    local res = 0 -- GPIO16

    spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, spi.DATABITS_8, 0)
    disp = ucg.ili9341_18x240x320_hw_spi(cs, dc, res)
end




-- switch statement http://lua-users.org/wiki/SwitchStatement
function switch(c)
    local swtbl = {
        casevar = c,
        caseof = function (self, code)
            local f
            if (self.casevar) then
                f = code[self.casevar] or code.default
            else
                f = code.missing or code.default
            end
            if f then
                if type(f)=="function" then
                    return f(self.casevar,self)
                else
                    error("case "..tostring(self.casevar).." not a function")
                end
            end
        end
    }
    return swtbl
end


z = 127   -- start value
function lcg_rnd()
    z = bit.band(65 * z + 17, 255)
    return z
end


function ucglib_graphics_test()
    --ucg.setMaxClipRange()
    disp:setColor(0, 0, 40, 80)
    disp:setColor(1, 80, 0, 40)
    disp:setColor(2, 255, 0, 255)
    disp:setColor(3, 0, 255, 255)
  
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 168, 0)
    disp:setPrintDir(0)
    disp:setPrintPos(2, 18)
    disp:print("Ucglib")
    disp:setPrintPos(2, 18+20)
    disp:print("GraphicsTest")
end

function cross()
    local mx, my
    disp:setColor(0, 250, 0, 0)
    disp:setColor(1, 255, 255, 30)
    disp:setColor(2, 220, 235, 10)
    disp:setColor(3, 205, 0, 30)
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())
    mx = disp:getWidth() / 2
    my = disp:getHeight() / 2

    disp:setColor(0, 255, 255, 255)
    disp:setPrintPos(2,18)
    disp:print("Cross")

    disp:setColor(0, 0, 0x66, 0xcc)
    disp:setPrintPos(mx+15, my-5)
    disp:print("dir0")
    disp:setPrintPos(mx+5, my+26)
    disp:print("dir1")
    disp:setPrintPos(mx-40, my+20)
    disp:print("dir2")
    disp:setPrintPos(mx+5,my-25)
    disp:print("dir3")

    disp:setColor(0, 0, 0x66, 0xff)
    disp:setColor(1, 0, 0x66, 0xcc)
    disp:setColor(2, 0, 0, 0x99)

    disp:draw90Line(mx+2, my-1, 20, 0, 0)
    disp:draw90Line(mx+2, my, 20, 0, 1)
    disp:draw90Line(mx+2, my+1, 20, 0, 2)

    disp:draw90Line(mx+1, my+2, 20, 1, 0)
    disp:draw90Line(mx, my+2, 20, 1, 1)
    disp:draw90Line(mx-1, my+2, 20, 1, 2)

    disp:draw90Line(mx-2, my+1, 20, 2, 0)
    disp:draw90Line(mx-2, my, 20, 2, 1)
    disp:draw90Line(mx-2, my-1, 20, 2, 2)

    disp:draw90Line(mx-1, my-2, 20, 3, 0)
    disp:draw90Line(mx, my-2, 20, 3, 1)
    disp:draw90Line(mx+1, my-2, 20, 3, 2)
end

function pixel_and_lines()
    local mx
    local x, xx
    mx = disp:getWidth() / 2
    --my = disp:getHeight() / 2
  
    disp:setColor(0, 0, 0, 150)
    disp:setColor(1, 0, 60, 40)
    disp:setColor(2, 60, 0, 40)
    disp:setColor(3, 120, 120, 200)
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 255, 255)
    disp:setPrintPos(2, 18)
    disp:setPrintDir(0)
    disp:print("Pix&Line")

    disp:drawPixel(0, 0)
    disp:drawPixel(1, 0)
    --disp:drawPixel(disp:getWidth()-1, 0)
    --disp:drawPixel(0, disp:getHeight()-1)
  
    disp:drawPixel(disp:getWidth()-1, disp:getHeight()-1)
    disp:drawPixel(disp:getWidth()-1-1, disp:getHeight()-1)

  
    x = 0
    while x < mx do
        xx = ((x)*255)/mx
        disp:setColor(255, 255-xx/2, 255-xx)
        disp:drawPixel(x, 24)
        disp:drawVLine(x+7, 26, 13)
        x = x + 1
    end
end

function color_test()
    local mx
    local c, x
    mx = disp:getWidth() / 2
    --my = disp:getHeight() / 2
  
    disp:setColor(0, 0, 0, 0)
    disp:drawBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 255, 255)
    disp:setPrintPos(2,18)
    disp:setPrintDir(0)
    disp:print("Color Test")

    disp:setColor(0, 127, 127, 127)
    disp:drawBox(0, 20, 16*4+4, 5*8+4)

    c = 0
    x = 2
    while c < 255 do
        disp:setColor(0, c, c, c)
        disp:drawBox(x, 22, 4, 8)
        disp:setColor(0, c, 0, 0)
        disp:drawBox(x, 22+8, 4, 8)
        disp:setColor(0, 0, c, 0)
        disp:drawBox(x, 22+2*8, 4, 8)
        disp:setColor(0, 0, 0, c)
        disp:drawBox(x, 22+3*8, 4, 8)
        disp:setColor(0, c, 255-c, 0)
        disp:drawBox(x, 22+4*8, 4, 8)

        c = c + 17
        x = x + 4
    end
end

function millis()
    local usec = tmr.now()
    return usec/1000
end

function triangle()
    local m
  
    disp:setColor(0, 0, 80, 20)
    disp:setColor(1, 60, 80, 20)
    disp:setColor(2, 60, 120, 0)
    disp:setColor(3, 0, 140, 30)  
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 255, 255)
    disp:setPrintPos(2, 18)
    disp:print("Triangle")

    m = millis() + T

    while millis() < m do
        disp:setColor(bit.band(lcg_rnd(), 127)+127, bit.band(lcg_rnd(), 31), bit.band(lcg_rnd(), 127)+64)
    
        disp:drawTriangle(
            bit.rshift(lcg_rnd() * (disp:getWidth()), 8),
            bit.rshift(lcg_rnd() * (disp:getHeight()-20), 8) + 20,
            bit.rshift(lcg_rnd() * (disp:getWidth()), 8),
            bit.rshift(lcg_rnd() * (disp:getHeight()-20), 8) + 20,
            bit.rshift(lcg_rnd() * (disp:getWidth()), 8),
            bit.rshift(lcg_rnd() * (disp:getHeight()-20), 8) + 20
        )

        tmr.wdclr()
    end
end

function fonts()
    local d = 5
    disp:setColor(0, 0, 40, 80)
    disp:setColor(1, 150, 0, 200)
    disp:setColor(2, 60, 0, 40)
    disp:setColor(3, 0, 160, 160)
  
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 255, 255)
    disp:setPrintDir(0)
    disp:setPrintPos(2,18)
    disp:print("Fonts")

    disp:setFontMode(ucg.FONT_MODE_TRANSPARENT)

    disp:setColor(255, 200, 170)
    disp:setFont(ucg.font_helvB08_hr)
    disp:setPrintPos(2,30+d)
    disp:print("ABC abc 123")
    disp:setFont(ucg.font_helvB10_hr)
    disp:setPrintPos(2,45+d)
    disp:print("ABC abc 123")
    disp:setFont(ucg.font_helvB12_hr)
    --disp:setPrintPos(2,62+d)
    --disp:print("ABC abc 123")
    disp:drawString(2,62+d, 0, "ABC abc 123") -- test drawString
  
    disp:setFontMode(ucg.FONT_MODE_SOLID)

    disp:setColor(255, 200, 170)
    disp:setColor(1, 0, 100, 120)		-- background color in solid mode
    disp:setFont(ucg.font_helvB08_hr)
    disp:setPrintPos(2,75+30+d)
    disp:print("ABC abc 123")
    disp:setFont(ucg.font_helvB10_hr)
    disp:setPrintPos(2,75+45+d)
    disp:print("ABC abc 123")
    disp:setFont(ucg.font_helvB12_hr)
    disp:setPrintPos(2,75+62+d)
    disp:print("ABC abc 123")

    disp:setFontMode(ucg.FONT_MODE_TRANSPARENT)

    disp:setFont(ucg.font_ncenR14_hr)
end

function set_clip_range()
    local x, y, w, h
    w = bit.band(lcg_rnd(), 31)
    h = bit.band(lcg_rnd(), 31)
    w = w + 25
    h = h + 25
    x = bit.rshift(lcg_rnd() * (disp:getWidth() - w), 8)
    y = bit.rshift(lcg_rnd() * (disp:getHeight() - h), 8)
  
    disp:setClipRange(x, y, w, h)
end

function loop()

    if (loop_idx == 0) then
        switch(bit.band(r, 3)) : caseof {
            [0]   = function() disp:undoRotate() end,
            [1]   = function() disp:setRotate90() end,
            [2]   = function() disp:setRotate180() end,
          default = function() disp:setRotate270() end
        }

        if ( r > 3 ) then
            disp:clearScreen()
            set_clip_range()
        end

        r = bit.band(r + 1, 255)
    end

    switch(loop_idx) : caseof {
        [0]   = function() end,
        [1]   = function() ucglib_graphics_test() end,
        [2]   = function() cross() end,
        [3]   = function() pixel_and_lines() end,
        [4]   = function() color_test() end,
        [5]   = function() triangle() end,
        [6]   = function() fonts() end,
      default = function() loop_idx = -1 end
    }

    loop_idx = loop_idx + 1
end


T = 1500

r = 0
loop_idx = 0

init_spi_display()

disp:begin(ucg.FONT_MODE_TRANSPARENT)
disp:setFont(ucg.font_ncenR14_hr)
disp:clearScreen()


tmr.register(0, 3000, tmr.ALARM_AUTO, function() loop() end)
tmr.start(0)
