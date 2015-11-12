local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component fonts...")

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

    print("...done")
end

return M
