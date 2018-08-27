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

The sender dumps the 2nd level FIFO aggregating records up to 1024 bytes and once this
is empty dumps an aggrate of the 1st level.

]]
local node, table, tmr, wifi, uwrite,     tostring = 
      node, table, tmr, wifi, uart.write, tostring

local function telnet_listener(socket) 
  local insert,       remove,       concat,       heap,      gc   = 
        table.insert, table.remove, table.concat, node.heap, collectgarbage
  local fifo1, fifo1l, fifo2, fifo2l = {}, 0, {}, 0                  
  local s -- s is a copy of the TCP socket if and only if sending is in progress

  local wdclr, cnt = tmr.wdclr, 0
  local function debug(fmt, ...)
    if (...) then fmt = fmt:format(...) end
    uwrite(0, "\r\nDBG: ",fmt,"\r\n" )
    cnt = cnt + 1
    if cnt % 10 then wdclr() end
  end

  local function flushGarbage()
    if heap() < 13440 then gc() end
  end

  local function sendLine()
 -- debug("entering sendLine")  
    if not s then return end

      if fifo2l + fifo1l == 0 then -- both FIFOs empty, so clear down s
        s = nil     
     -- debug("Q cleared")        
        return
    end

    flushGarbage()
  
    if #fifo2 < 4 then -- Flush FIFO1 into FIFO2
      insert(fifo2,concat(fifo1))
   -- debug("flushing %u bytes / %u recs of FIFO1 into FIFO2[%u]", fifo1l, #fifo1, #fifo2) 
      fifo2l, fifo1, fifo1l = fifo2l + fifo1l, {}, 0
	  end

    -- send out first 4 FIFO2 recs (or all if #fifo2<5)  
    local rec =  remove(fifo2,1)        .. (remove(fifo2,1) or '') ..
                (remove(fifo2,1) or '') .. (remove(fifo2,1) or '')
    fifo2l = fifo2l - #rec

    flushGarbage()
    s:send(rec)
 -- debug( "sending %u bytes (%u buffers remain)\r\n%s ", #rec, #fifo2, rec)
  end
  local F1_SIZE = 256
  local function queueLine(str)
    -- Note that this algo does work for strings longer than 256 but it is sub-optimal
    -- as it does string splitting, but this isn't really an issue IMO, as in practice 
    -- fields of this size are very infrequent.
 
 -- debug("entering queueLine(l=%u)", #str)  

    while #str > 0 do  -- this is because str might be longer than the packet size!
      local k, l = F1_SIZE - fifo1l, #str
      local chunk

      -- Is it time to batch up and flush FIFO1 into a new FIFO2 entry? Note that it's
      -- not worth splitting a string to squeeze the last ounce out of a buffer size.

   -- debug("#fifo1 = %u, k = %u, l = %u", #fifo1, k, l) 
      if #fifo1 >= 32 or (k < l and k < 16) then 
        insert(fifo2, concat(fifo1))
     -- debug("flushing %u bytes / %u recs of FIFO1 into FIFO2[%u]", fifo1l, #fifo1, #fifo2) 
        fifo2l, fifo1, fifo1l, k = fifo2l + fifo1l, {}, 0, F1_SIZE
      end

      if l > k+16 then -- also tolerate a size overrun of 16 bytes to avoid a split 
        chunk, str = str:sub(1,k), str:sub(k+1)
      else
        chunk, str = str, ''
      end
  
   -- debug("pushing %u bytes into FIFO1[l=%u], %u bytes remaining", #chunk, fifo1l, #str) 
      insert(fifo1, chunk)
      fifo1l = fifo1l + #chunk
    end

    if not s and socket then
      s = socket
      sendLine()      
    else
      flushGarbage()
    end

  end

  local function receiveLine(s, line)
 -- debug( "received: %s", line) 
    node.input(line)
  end

  local function disconnect(s)
    fifo1, fifo1l, fifo2, fifo2l, s = {}, 0, {}, 0, nil
    node.output(nil)
  end

  socket:on("receive",       receiveLine)
  socket:on("disconnection", disconnect)
  socket:on("sent",          sendLine)
  node.output(queueLine, 0)
end

local listenerSocket
return {
  open = function(this, ssid, pwd, port)
    if ssid then
      wifi.setmode(wifi.STATION, false)
      wifi.sta.config { ssid = ssid, pwd  = pwd, save = false }
    end
    tmr.alarm(0, 500, tmr.ALARM_AUTO, function()
      if (wifi.sta.status() == wifi.STA_GOTIP) then
        tmr.unregister(0)
        print("Welcome to NodeMCU world", node.heap(), wifi.sta.getip())
        net.createServer(net.TCP, 180):listen(port or 2323, telnet_listener)
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
