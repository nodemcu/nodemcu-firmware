-- ****************************************************************************
-- Network streaming example
--
-- stream = require("play_network")
-- stream.init(pin)
-- stream.play(pcm.RATE_8K, ip, port, "/jump_8k.u8", function () print("stream finished") end)
--
-- Playback can be stopped with stream.stop().
-- And resources are free'd with stream.close().
--
local M, module = {}, ...
_G[module] = M

local _conn
local _drv
local _buf
local _lower_thresh = 2
local _upper_thresh = 5
local _play_cb


-- ****************************************************************************
-- PCM 
local function stop_stream(cb)
  _drv:stop()
  if _conn then
    _conn:close()
    _conn = nil
  end
  _buf = nil
  if cb then cb()
  elseif _play_cb then _play_cb()
  end
  _play_cb = nil
end

local function cb_drained()
  print("drained "..node.heap())
  stop_stream()
end

local function cb_data()
  if #_buf > 0 then
    local data = table.remove(_buf, 1)

    if #_buf <= _lower_thresh then
      -- unthrottle server to get further data into the buffer
      _conn:unhold()
    end
    return data
  end
end

local _rate
local function start_play()
  print("starting playback")

  -- start playback
  _drv:play(_rate)
end


-- ****************************************************************************
-- Networking functions
--
local _skip_headers
local _chunk
local _buffering
local function data_received(c, data)

  if _skip_headers then
    -- simple logic to filter the HTTP headers
    _chunk = _chunk..data
    local i, j = string.find(_chunk, '\r\n\r\n')
    if i then
      _skip_headers = false
      data = string.sub(_chunk, j+1, -1)
      _chunk = nil
    end
  end

  if not _skip_headers then
    _buf[#_buf+1] = data

    if #_buf > _upper_thresh then
      -- throttle server to avoid buffer overrun
      c:hold()
      if _buffering then
        -- buffer got filled, start playback
        start_play()
        _buffering = false
      end
    end
  end

end

local function cb_disconnected()
  if _buffering then
    -- trigger playback when disconnected but we're still buffering
    start_play()
    _buffering = false
  end
end

local _path
local function cb_connected(c)
  c:send("GET ".._path.." HTTP/1.0\r\nHost: iot.nix.nix\r\n".."Connection: close\r\nAccept: /\r\n\r\n")
  _path = nil
end


function M.play(rate, ip, port, path, cb)
  _skip_headers = true
  _chunk = ""
  _buffering = true
  _buf = {}
  _rate = rate
  _path = path
  _play_cb = cb

  _conn = net.createConnection(net.TCP, 0)
  _conn:on("receive", data_received)
  _conn:on("disconnection", cb_disconnected)
  _conn:connect(port, ip, cb_connected)
end

function M.stop(cb)
  stop_stream(cb)
end

function M.init(pin)
  _drv = pcm.new(pcm.SD, pin)

  -- get called back when all samples were read from stream
  _drv:on("drained", cb_drained)
  _drv:on("data", cb_data)

  --_drv:on("stopped", cb_stopped)
end

function M.vu(cb, freq)
 _drv:on("vu", cb, freq)
end

function M.close()
  stop_stream()
  _drv:close()
  _drv = nil
end

return M
