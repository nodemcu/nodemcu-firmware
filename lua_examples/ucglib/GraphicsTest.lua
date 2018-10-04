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
        [1]   = function() require("GT_graphics_test").run() end,
        [2]   = function() require("GT_cross").run() end,
        [3]   = function() require("GT_pixel_and_lines").run() end,
        [4]   = function() require("GT_color_test").run() end,
        [5]   = function() require("GT_triangle").run() end,
        [6]   = function() require("GT_fonts").run() end,
        [7]   = function() require("GT_text").run() end,
        [8]   = function() if r <= 3 then require("GT_clip").run() end end,
        [9]   = function() require("GT_box").run() end,
        [10]  = function() require("GT_gradient").run() end,
        [11]  = function() disp:setMaxClipRange() end,
      default = function() loop_idx = -1 end
    }

    loop_idx = loop_idx + 1

    print("Heap: " .. node.heap())
end


T = 1000

r = 0
loop_idx = 0

init_spi_display()

disp:begin(ucg.FONT_MODE_TRANSPARENT)
disp:setFont(ucg.font_ncenR14_hr)
disp:clearScreen()

milli = 0
function millis()
   milli = milli + 30
   return milli
end

t = tmr.create()
t:register(5000, tmr.ALARM_AUTO, function() loop() end)
t:start()
