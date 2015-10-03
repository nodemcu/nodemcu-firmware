local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component clip...")

    disp:setColor(0, 0x00, 0xd1, 0x5e)  -- dark green
    disp:setColor(1, 0xff, 0xf7, 0x61)  -- yellow
    disp:setColor(2, 0xd1, 0xc7, 0x00)  -- dark yellow
    disp:setColor(3, 0x61, 0xff, 0xa8)  -- green
  
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 255, 255)
    disp:setPrintPos(2,18)
    disp:setPrintDir(0)
    disp:print("ClipRange")
  
    disp:setColor(0xd1, 0x00, 0x073)
  
    disp:setFont(ucg.font_helvB18_hr)
  
    disp:setPrintPos(25,45)
    disp:setPrintDir(0)
    disp:print("Ucg")
    disp:setPrintDir(1)
    disp:print("Ucg")
    disp:setPrintDir(2)
    disp:print("Ucg")
    disp:setPrintDir(3)
    disp:print("Ucg")
  
  
    disp:setMaxClipRange()
    disp:setColor(0xff, 0xff, 0xff)
    disp:drawFrame(20-1,30-1,15+2,20+2)
    disp:setClipRange(20, 30, 15, 20)
    disp:setColor(0xff, 0x61, 0x0b8)
    disp:setPrintPos(25,45)
    disp:setPrintDir(0)
    disp:print("Ucg")
    disp:setPrintDir(1)
    disp:print("Ucg")
    disp:setPrintDir(2)
    disp:print("Ucg")
    disp:setPrintDir(3)
    disp:print("Ucg")
  

    disp:setMaxClipRange()
    disp:setColor(0xff, 0xff, 0xff)
    disp:drawFrame(60-1,35-1,25+2,18+2)
    disp:setClipRange(60, 35, 25, 18)
    disp:setColor(0xff, 0x61, 0x0b8)
    disp:setPrintPos(25,45)
    disp:setPrintDir(0)
    disp:print("Ucg")
    disp:setPrintDir(1)
    disp:print("Ucg")
    disp:setPrintDir(2)
    disp:print("Ucg")
    disp:setPrintDir(3)
    disp:print("Ucg")

    disp:setMaxClipRange()
    disp:setColor(0xff, 0xff, 0xff)
    disp:drawFrame(7-1,58-1,90+2,4+2)  
    disp:setClipRange(7, 58, 90, 4)
    disp:setColor(0xff, 0x61, 0x0b8)
    disp:setPrintPos(25,45)
    disp:setPrintDir(0)
    disp:print("Ucg")
    disp:setPrintDir(1)
    disp:print("Ucg")
    disp:setPrintDir(2)
    disp:print("Ucg")
    disp:setPrintDir(3)
    disp:print("Ucg")

    disp:setFont(ucg.font_ncenR14_hr)
    disp:setMaxClipRange()

    print("...done")
end

return M
