------------------------------------------------------------------------------
-- Net send queueing helper
--
-- See also
--   https://nodemcu.readthedocs.org/en/dev/en/modules/net/#netsocketsend
--
-- Based on Vladimir Dronnikov's
--   MQTT queuing publish helper
--   https://github.com/dvv/nodemcu-thingies/blob/master/mqtt-queue-helper.lua
--   LICENCE: http://opensource.org/licenses/MIT
--
-- Send arbitrary payload:
--   local socket = net.createConnection(net.TCP, 0)
--   socket:connect(port, ip)
--   local net_send = dofile("net_send_queue.lua")(socket)
--   net_send(payload1)
--   net_send(payload2)
--   net_send(payload3)
--
------------------------------------------------------------------------------
do
  -- cache
  local shift = table.remove
  -- queue handler
  local make_sender = function(socket)
    local queue = { }
    local is_sending = false
    local function send()
      if #queue > 0 then
        local item = shift(queue, 1)
        socket:send(item, send)
      else
        is_sending = false
      end
    end
    return function(...)
      queue[#queue + 1] = ...
      if not is_sending then
        is_sending = true
        send()
      end
    end
  end
  -- expose
  return make_sender
end
