------------------------------------------------------------------------------
-- Trigonometry module
--
--  tg = dofile("trigonometry.lua")
-- val = tg.tan(rad)
-- val = tg.sin(rad)
-- val = tg.cos(rad)
-- rad = tg.atan(x)
-- rad = tg.atan2(y, x)
-- rad = tg.asin(x)
-- rad = tg.acos(x)
-- deg = tg.deg(rad)
-- rad = tg.rad(deg)
--  pi = tg.pi
------------------------------------------------------------------------------
local M
do
  -- const
  local PI = 3.14159265358979323846
  local PI14 = 0.25 * PI
  local PI24 = 0.50 * PI
  local PI34 = 0.75 * PI
  local PI54 = 1.25 * PI
  local PI64 = 1.50 * PI
  local PI74 = 1.75 * PI
  local PI84 = 2.00 * PI
  local PI112 = PI / 12
  local PI16  = PI / 6
  local SQRT3 = 1.73205081
  local ATAN1 = 1.40878120
  local ATAN2 = 0.55913709
  local ATAN3 = 0.60310579
  local ATAN4 = 0.05160454

  -- val = kernel(x)
  local kernel = function(x)
    local t = 0
    for n = 3, 0, -1 do
      if n > 0 then
        t = (x * x) / (2 * n + 1 - t)
      else
        t = x / (1 - t)
      end
    end
    return t
  end

  -- val = M.tan(x)
  local tan = function(x)
    if x <= PI14 then
      local t = kernel(0.5 * x)
      return (2 * t) / (1 - t * t)
    elseif x <= PI24 then
      local t = kernel(0.5 * (PI24 - x))
      return (1 - t * t) / (2 * t)
    elseif x <= PI34 then
      local t = kernel(0.5 * (x - PI24))
      return (-1) * (1 - t * t) / (2 * t)
    else -- if x <= PI then
      local t = kernel(0.5 * (PI - x))
      return (-1) * (2 * t) / (1 - t * t)
    end
  end
  
  -- val = M.sin(x)
  local sin = function(x)
    if x <= PI14 then
      local t = kernel(0.5 * x)
      return (2 * t) / (1 + t * t)
    elseif x <= PI24 then
      local t = kernel(0.5 * (PI24 - x))
      return (1 - t * t) / (1 + t * t)
    elseif x <= PI34 then
      local t = kernel(0.5 * (x - PI24))
      return (1 - t * t) / (1 + t * t)
    elseif x <= PI then
      local t = kernel(0.5 * (PI - x))
      return (2 * t) / (1 + t * t)
    elseif x <= PI54 then
      local t = kernel(0.5 * (x - PI))
      return (-1) * (2 * t) / (1 + t * t)
    elseif x <= PI64 then
      local t = kernel(0.5 * (PI64 - x))
      return (-1) * (1 - t * t) / (1 + t * t)
    elseif x <= PI74 then
      local t = kernel(0.5 * (x - PI64))
      return (-1) * (1 - t * t) / (1 + t * t)
    else -- if x <= PI84 then
      local t = kernel(0.5 * (PI84 - x))
      return (-1) * (2 * t) / (1 + t * t)
    end
  end

  -- val = M.cos(x)
  local cos = function(x)
    if x <= PI14 then
      local t = kernel(0.5 * x)
      return (1 - t * t) / (1 + t * t)
    elseif x <= PI24 then
      local t = kernel(0.5 * (PI24 - x))
      return (2 * t) / (1 + t * t)
    elseif x <= PI34 then
      local t = kernel(0.5 * (x - PI24))
      return (-1) * (2 * t) / (1 + t * t)
    elseif x <= PI then
      local t = kernel(0.5 * (PI - x))
      return (-1) * (1 - t * t) / (1 + t * t)
    elseif x <= PI54 then
      local t = kernel(0.5 * (x - PI))
      return (-1) * (1 - t * t) / (1 + t * t)
    elseif x <= PI64 then
      local t = kernel(0.5 * (PI64 - x))
      return (-1) * (2 * t) / (1 + t * t)
    elseif x <= PI74 then
      local t = kernel(0.5 * (x - PI64))
      return (2 * t) / (1 + t * t)
    else -- if x <= PI84 then
      local t = kernel(0.5 * (PI84 - x))
      return (1 - t * t) / (1 + t * t)
    end
  end

  -- rad = M.atan(x)
  local atan = function(x)
    local sign, inv, sp = false, false, 0
    if x < 0 then
      x = -x
      sign = true
    end
    if x > 1 then
      x = 1 / x
      inv = true
    end
    while x > PI112 do
      x = ((x * SQRT3) - 1) * (1 / (x + SQRT3))
      sp = sp + 1
    end
    local X2 = x ^ 2
    local at = ((ATAN2 / (X2 + ATAN1)) + ATAN3 - (ATAN4 * X2)) * x
    while sp > 0 do
      at = at + PI16
      sp = sp - 1
    end
    if inv then
      at = PI24 - at
    end
    if sign then
      at = -at
    end
    return at
  end

  -- rad = M.atan2(y, x)
  local atan2 = function(y, x)
    if y == 0 then
      return y
    elseif x == 0 then
      if y < 0 then
        return -PI24
      else
        return PI24
      end
    elseif x == 1 then
      return atan(y)
    else
      if x > 0 then
        if y > 0 then
          return atan(y / x)
        else
          return (-1) * atan(-y / x)
        end
      else
        if y > 0 then
          return PI - atan(y / -x)
        else
          return atan(-y / -x) - PI
        end
      end
    end
  end

  -- rad = M.asin(x)
  local asin = function(x)
    if x <= -1 then
      return -PI24
    elseif x >= 1 then
      return PI24
    else
      return atan(x / ((1 - (x ^ 2)) ^ 0.5))
    end
  end

  -- rad = M.acos(x)
  local acos = function(x)
    return PI24 - asin(x)
  end

  -- deg = M.deg(rad)
  local deg = function(rad)
    return rad * 180 / PI
  end

  -- rad = M.rad(deg)
  local rad = function(deg)
    return deg * PI / 180
  end

  -- expose
  M = {
    tan = tan,
    sin = sin,
    cos = cos,
    atan = atan,
    atan2 = atan2,
    asin = asin,
    acos = acos,
    deg = deg,
    rad = rad,
    pi = PI,
  }
end
return M
