local M, module = {}, ...
_G[module] = M

function M.run()
    -- make this a volatile module:
    package.loaded[module] = nil

    print("Running component box...")

    local x, y, w, h
    local m
  
    disp:setColor(0, 0, 40, 80)
    disp:setColor(1, 60, 0, 40)
    disp:setColor(2, 128, 0, 140)
    disp:setColor(3, 0, 128, 140) 
    disp:drawGradientBox(0, 0, disp:getWidth(), disp:getHeight())

    disp:setColor(255, 255, 255)
    disp:setPrintPos(2,18)
    disp:setPrintDir(0)
    disp:print("Box")

    m = millis() + T

  while millis() < m  do
      disp:setColor(bit.band(lcg_rnd(), 127)+127, bit.band(lcg_rnd(), 127)+64, bit.band(lcg_rnd(), 31))
      w = bit.band(lcg_rnd(), 31)
      h = bit.band(lcg_rnd(), 31)
      w = w + 10
      h = h + 10
      x = bit.rshift(lcg_rnd() * (disp:getWidth()-w), 8)
      y = bit.rshift(lcg_rnd() * (disp:getHeight()-h-20), 8)
    
      disp:drawBox(x, y+20, w, h)
  end

  print("...done")
end

return M
