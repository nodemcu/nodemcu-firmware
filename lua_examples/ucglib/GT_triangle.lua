local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component triangle...")

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

    print("...done")
end

return M
