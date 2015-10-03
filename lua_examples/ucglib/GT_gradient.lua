local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component gradient...")

    disp:setColor(0, 0, 255, 0)
    disp:setColor(1, 255, 0, 0)
    disp:setColor(2, 255, 0, 255)
    disp:setColor(3, 0, 255, 255)

    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())
  
    disp:setColor(255, 255, 255)
    disp:setPrintPos(2,18)
    disp:setPrintDir(0)
    disp:print("GradientBox")

    disp:setColor(0, 0, 255, 0)
    disp:drawBox(2, 25, 8, 8)

    disp:setColor(0, 255, 0, 0)
    disp:drawBox(2+10, 25, 8, 8)

    disp:setColor(0, 255, 0, 255)
    disp:drawBox(2, 25+10, 8, 8)

    disp:setColor(0, 0, 255, 255)
    disp:drawBox(2+10, 25+10, 8, 8)

    print("...done")
end

return M
