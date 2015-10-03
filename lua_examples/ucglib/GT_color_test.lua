local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component color_test...")

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

    print("...done")
end

return M
