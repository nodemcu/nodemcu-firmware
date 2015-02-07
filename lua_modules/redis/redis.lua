------------------------------------------------------------------------------
-- Redis client module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
--
-- Example:
-- local redis = dofile("redis.lua").connect(host, port)
-- redis:publish("chan1", foo")
-- redis:subscribe("chan1", function(channel, msg) print(channel, msg) end)
------------------------------------------------------------------------------
local M
do
  -- const
  local REDIS_PORT = 6379
  -- cache
  local pairs, tonumber = pairs, tonumber
  --
  local publish = function(self, chn, s)
    self._fd:send(("*3\r\n$7\r\npublish\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n"):format(
        #chn, chn, #s, s
      ))
    -- TODO: confirmation? then queue of answers needed
  end
  local subscribe = function(self, chn, handler)
    -- TODO: subscription to all channels, with single handler
    self._fd:send(("*2\r\n$9\r\nsubscribe\r\n$%d\r\n%s\r\n"):format(
        #chn, chn
      ))
    self._handlers[chn] = handler
    -- TODO: confirmation? then queue of answers needed
  end
  local unsubscribe = function(self, chn)
    self._handlers[chn] = false
  end
  -- NB: pity we can not just augment what net.createConnection returns
  local close = function(self)
    self._fd:close()
  end
  local connect = function(host, port)
    local _fd = net.createConnection(net.TCP, 0)
    local self = {
      _fd = _fd,
      _handlers = { },
      -- TODO: consider metatables?
      close = close,
      publish = publish,
      subscribe = subscribe,
      unsubscribe = unsubscribe,
    }
    _fd:on("connection", function()
      --print("+FD")
    end)
    _fd:on("disconnection", function()
      -- FIXME: this suddenly occurs. timeout?
      --print("-FD")
    end)
    _fd:on("receive", function(fd, s)
      --print("IN", s)
      -- TODO: subscription to all channels
      -- lookup message pattern to determine channel and payload
      -- NB: pairs() iteration gives no fixed order!
      for chn, handler in pairs(self._handlers) do
        local p = ("*3\r\n$7\r\nmessage\r\n$%d\r\n%s\r\n$"):format(#chn, chn)
        if s:find(p, 1, true) then
          -- extract and check message length
          -- NB: only the first TCP packet considered!
          local _, start, len = s:find("(%d-)\r\n", #p)
          if start and tonumber(len) == #s - start - 2 and handler then
            handler(chn, s:sub(start + 1, -2)) -- ends with \r\n
          end
        end
      end
    end)
    _fd:connect(port or REDIS_PORT, host)
    return self
  end
  -- expose
  M = {
    connect = connect,
  }
end
return M
