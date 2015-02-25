------------------------------------------------------------------------------
-- IR send module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
--
-- Example:
-- dofile("irsend.lua").nec(4, 0x00ff00ff)
------------------------------------------------------------------------------
local M
do
  -- const
  local NEC_PULSE_US   = 1000000 / 38000
  local NEC_HDR_MARK   = 9000
  local NEC_HDR_SPACE  = 4500
  local NEC_BIT_MARK   =  560
  local NEC_ONE_SPACE  = 1600
  local NEC_ZERO_SPACE =  560
  local NEC_RPT_SPACE  = 2250
  -- cache
  local gpio, bit = gpio, bit
  local mode, write = gpio.mode, gpio.write
  local waitus = tmr.delay
  local isset = bit.isset
  -- NB: poorman 38kHz PWM with 1/3 duty. Touch with care! )
  local carrier = function(pin, c)
    c = c / NEC_PULSE_US
    while c > 0 do
      write(pin, 1)
      write(pin, 0)
      c = c + 0
      c = c + 0
      c = c + 0
      c = c + 0
      c = c + 0
      c = c + 0
      c = c * 1
      c = c * 1
      c = c * 1
      c = c - 1
    end
  end
  -- tsop signal simulator
  local pull = function(pin, c)
    write(pin, 0)
    waitus(c)
    write(pin, 1)
  end
  -- NB: tsop mode allows to directly connect pin
  --     inplace of TSOP input
  local nec = function(pin, code, tsop)
    local pulse = tsop and pull or carrier
    -- setup transmitter
    mode(pin, 1)
    write(pin, tsop and 1 or 0)
    -- header
    pulse(pin, NEC_HDR_MARK)
    waitus(NEC_HDR_SPACE)
    -- sequence, lsb first
    for i = 31, 0, -1 do
      pulse(pin, NEC_BIT_MARK)
      waitus(isset(code, i) and NEC_ONE_SPACE or NEC_ZERO_SPACE)
    end
    -- trailer
    pulse(pin, NEC_BIT_MARK)
    -- done transmitter
    --mode(pin, 0, tsop and 1 or 0)
  end
  -- expose
  M = {
    nec = nec,
  }
end
return M
