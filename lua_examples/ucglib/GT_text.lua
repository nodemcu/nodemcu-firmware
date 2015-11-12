local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component text...")

    local x, y, w, h, i
    local m
  
    disp:setColor(0, 80, 40, 0)
    disp:setColor(1, 60, 0, 40)
    disp:setColor(2, 20, 0, 20)
    disp:setColor(3, 60, 0, 0)
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 255, 255)
    disp:setPrintPos(2,18)
    disp:setPrintDir(0)
    disp:print("Text")

    m = millis() + T
    i = 0
    while millis() < m do
        disp:setColor(bit.band(lcg_rnd(), 31), bit.band(lcg_rnd(), 127) + 127, bit.band(lcg_rnd(), 127) + 64)
        w = 40
        h = 22
        x = bit.rshift(lcg_rnd() * (disp:getWidth() - w), 8)
        y = bit.rshift(lcg_rnd() * (disp:getHeight() - h), 8)
    
        disp:setPrintPos(x, y+h)
        disp:setPrintDir(bit.band(bit.rshift(i, 2), 3))
        i = i + 1
        disp:print("Ucglib")
    end
    disp:setPrintDir(0)

    print("...done")
end

return M
