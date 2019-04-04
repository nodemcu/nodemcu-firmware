-- Wrap a two-staged fifo around a socket's send; see
-- docs/lua-modules/fifosock.lua for more documentation.
--
-- See fifosocktest.lua for some examples of use or tricky cases.
--
-- Our fifos can take functions; these can be useful for either lazy
-- generators or callbacks for parts of the stream having been sent.

local BIGTHRESH = 256   -- how big is a "big" string?
local SPLITSLOP = 16    -- any slop in the big question?
local FSMALLLIM = 32    -- maximum number of small strings held
local COALIMIT  = 3

local concat = table.concat
local insert = table.insert
local gc     = collectgarbage

local function wrap(sock)
  -- the two fifos
  local fsmall, lsmall, fbig = {}, 0, (require "fifo").new()

  -- ssend last aggregation string and aggregate count
  local ssla, sslan = nil, 0
  local ssend  = function(s,islast)
    local ns = nil

    -- Optimistically, try coalescing FIFO dequeues.  But, don't try to
    -- coalesce function outputs, since functions might be staging their
    -- execution on the send event implied by being called.

    if type(s) == "function" then
      if sslan ~= 0 then
        sock:send(ssla)
        ssla, sslan = nil, 0; gc()
        return s, false -- stay as is and wait for :on("sent")
      end
      s, ns = s()
    elseif type(s) == "string" and sslan < COALIMIT then
      if sslan == 0
       then ssla, sslan = s, 1
       else ssla, sslan = ssla .. s, sslan + 1
      end
      if islast then
        -- this is shipping; if there's room, steal the small fifo, too
        if sslan < COALIMIT then
          sock:send(ssla .. concat(fsmall))
          fsmall, lsmall = {}, 0
        else
          sock:send(ssla)
        end
        ssla, sslan = "", 0; gc()
        return nil, false
      else
        return nil, true
      end
    end

    -- Either that was a function or we've hit our coalescing limit or
    -- we didn't ship above.  Ship now, if there's something to ship.
    if s ~= nil then
      if sslan == 0 then sock:send(s) else sock:send(ssla .. s) end
      ssla, sslan = nil, 0; gc()
      return ns or nil, false
    elseif sslan ~= 0 then
      assert (ns == nil)
      sock:send(ssla)
      ssla, sslan = nil, 0; gc()
      return nil, false
    else
      assert (ns == nil)
      return nil, true
    end
  end

  -- Move fsmall to fbig; might send if fbig empty
  local function promote(f)
    if #fsmall == 0 then return end
    local str = concat(fsmall)
    fsmall, lsmall = {}, 0
    fbig:queue(str, f or ssend)
  end

  local function sendnext()
    if not fbig:dequeue(ssend) then promote() end
  end

  sock:on("sent", sendnext)

  return function(s)
    -- don't sweat the petty things
    if s == nil or s == "" then return end

    -- Function?  Go ahead and queue this thing in the right place.
    if type(s) == "function" then promote(); fbig:queue(s, ssend); return; end

    s = tostring(s)

    -- cork sending until the end in case we're the head of line
    local corked = false
    local function corker(t) corked = true; return t end

    -- small fifo would overfill?  promote it
    if lsmall + #s > BIGTHRESH or #fsmall >= FSMALLLIM then promote(corker) end

    -- big string?  chunk and queue big components immediately
    -- behind any promotion that just took place
    while #s > BIGTHRESH + SPLITSLOP do
     local pfx
     pfx, s = s:sub(1,BIGTHRESH), s:sub(BIGTHRESH+1)
     fbig:queue(pfx, corker)
    end

    -- Big string?  queue and maybe tx now
    if #s > BIGTHRESH then fbig:queue(s, corker)
    -- small and fifo in immediate dequeue mode
    elseif fbig._go and lsmall == 0 then fbig:queue(s, corker)
    -- small and queue already moving; let it linger in the small fifo
    else insert(fsmall, s) ; lsmall = lsmall + #s
    end

    -- if it happened that we corked the transmission above...
    --   if we queued a good amount of data, go ahead and start transmitting;
    --   otherwise, wait a tick and hopefully we will queue more in the interim
    --   before transmitting.
    if corked then
      if #fbig <= COALIMIT
       then tmr.create():alarm(1, tmr.ALARM_SINGLE, sendnext)
       else sendnext()
      end
    end
  end
end

return { wrap = wrap }
