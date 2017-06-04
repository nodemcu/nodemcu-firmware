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
  -- queue handler
  local make_sender = function(socket)
    local shift = table.remove
    local max_send, queue, is_sending = 1460, { }, false

    local function send()
      if #queue > 0 then
        local item = shift(queue, 1)
        socket:send(item, send)
      else
        is_sending = false
      end
    end

    return function(send_data)
      -- handle max send size
      while #send_data > max_send do
        queue[#queue + 1] = send_data:sub(1, max_send)
        send_data = send_data:sub(max_send + 1)
      end
      if #send_data > 0 then
        queue[#queue + 1] = send_data
      end
      if not is_sending then
        is_sending = true
        send()
      end
    end

  end
  -- expose
  return make_sender
end
