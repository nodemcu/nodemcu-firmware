--[[  A telnet server   T. Ellison,  May 2018

This version is more complex than the simple Lua example previously provided in our
distro.  The main reason is that a single Lua command can produce a LOT of output,
and the server has to work within four constraints:

  -  The SDK rules are that you can only issue one send per task invocation, so any
     overflow must be buffered, and the buffer emptied using an on:sent cb

  -  Since the interpeter invokes a node.output cb per field, you have a double
     whammy that these fields are typically small, so using a simple array FIFO
     would rapidly exhaust RAM.

  -  For network efficiency, you want to aggregate any FIFO buffered into sensible
     sized packet, say 1024 bytes, but you also need to handle the case when larger
     string span multiple packets. However, you must flush the buffer if necessary.

  -  The overall buffering strategy needs to be reasonably memory efficient and avoid
     hitting the GC too hard, so where practical avoid aggregating small strings to
     more than 256 chars (as NodeMCU handles <256 using stack buffers), and avoid
     serial aggregation such as buf = buf .. str as this hammers the GC.

So this server adopts a simple buffering scheme using a two level FIFO. The node.output
cb adds cb records to the 1st level FIFO until the #recs is > 32 or the total size
would exceed 256 bytes. Once over this threashold, the contents of the FIFO are
concatenated into a 2nd level FIFO entry of upto 256 bytes, and the 1st level FIFO
cleared down to any residue.

]]
local node, table, tmr, wifi, uwrite,     tostring =
      node, table, tmr, wifi, uart.write, tostring

local function telnet_listener(socket) 
  local queueLine = (require "fifosock").wrap(socket)

  local function receiveLine(s, line)
    node.input(line)
  end

  local function disconnect(s)
    socket:on("disconnection", nil)
    socket:on("reconnection", nil)
    socket:on("connection", nil)
    socket:on("receive", nil)
    socket:on("sent", nil)
    node.output(nil)
  end

  socket:on("receive",       receiveLine)
  socket:on("disconnection", disconnect)
  node.output(queueLine, 0)
  print(("Welcome to NodeMCU world (%d mem free, %s)"):format(node.heap(), wifi.sta.getip()))
end

local listenerSocket
return {
  open = function(this, ssid, pwd, port)
    if ssid then
      wifi.setmode(wifi.STATION, false)
      wifi.sta.config { ssid = ssid, pwd  = pwd, save = false }
    end
    local t = tmr.create()
    t:alarm(500, tmr.ALARM_AUTO, function()
      if (wifi.sta.status() == wifi.STA_GOTIP) then
        t:unregister()
        t=nil
        print(("Telnet server started (%d mem free, %s)"):format(node.heap(), wifi.sta.getip()))
        net.createServer(net.TCP, 180):listen(port or 23, telnet_listener)
      else
        uwrite(0,".")
      end
    end)
  end,

  close = function(this)
    if listenerSocket then
      listenerSocket:close()
      package.loaded.telnet = nil
      listenerSocket = nil
      collectgarbage()
    end
  end,
}
