local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component graphics_test...")

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

    print("...done")
end

return M
