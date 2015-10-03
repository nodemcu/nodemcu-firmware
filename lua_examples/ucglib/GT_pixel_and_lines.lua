local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component pixel_and_lines...")

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

    print("...done")
end

return M
