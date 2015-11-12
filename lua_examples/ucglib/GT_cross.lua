local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component cross...")

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

    print("...done")
end

return M
