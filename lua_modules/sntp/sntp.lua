-- Constructor
--   sc and fc are our Success and Failure Callbacks, resp.
local new = function(serv, sc, fc, now)

  if     type(serv) == "string" then serv = {serv}
  elseif serv == nil            then serv =
    {
      nil,
      "1.nodemcu.pool.ntp.org",
      "2.nodemcu.pool.ntp.org",
    }
    local ni = net.ifinfo(0)
    ni = ni and ni.dhcp
    serv[1] = ni.ntp_server or "0.nodemcu.pool.ntp.org"
  elseif type(serv) ~= "table"  then error "Bad server table"
  end

  if type(sc) ~= "function" then error "Bad success callback type" end
  if fc ~= nil and type(fc) ~= "function" then error "Bad failure callback type" end
  if now ~= nil and type(now) ~= "function" then error "Bad clock type" end

  now = now or (rtctime and rtctime.get)
  if now == nil then error "Need clock function" end

  local _self = {servers = serv}

  local _tmr -- contains the currently running timer, if any
  local _udp -- the socket we're using to talk to the world

  local _kod -- kiss of death flags accumulated accoss syncs
  local _pbest -- best server from prior pass

  local _res -- the best result we've got so far this pass
  local _best -- best server this pass, for updating _pbest

  local _six -- index of the server in serv to whom we are speaking
  local _sat -- number of times we've tried to reach this server

  -- Shut down the state machine
  --
  -- upvals: _tmr, _udp, _six, _sat, _res, _best
  local function _stop()
    -- stop any time-based callbacks and drop _tmr
    _tmr = _tmr and _tmr:unregister()

    _six, _sat, _res, _best = nil, nil, nil, nil

    -- stop any UDP callbacks and drop the socket; to be safe against
    -- knots tied in the registry, explicitly unregister callbacks first
    if _udp then
      _udp:on("receive", nil)
      _udp:on("sent"   , nil)
      _udp:on("dns"    , nil)
      _udp:close()
      _udp = nil
    end

    -- Count down _kod entries
    if _kod then
      for k,v in pairs(_kod) do _kod[k] = (v > 0) and (v - 1) or nil end
      if #_kod == 0 then _kod = nil end
    end
  end

  local nextServer
  local doserver

  -- Try communicating with the current server
  --
  -- upvals: now, _tmr, _udp, _best, _kod, _pbest, _res, _six
  local function hail(ip)
    _tmr:alarm(5000 --[[const param: SNTP_TIMEOUT]], tmr.ALARM_SINGLE, function()
      _udp:on("sent", nil)
      _udp:on("receive", nil)
      return doserver("timeout")
    end)

    local txts = sntppkt.make_ts(now())

    _udp:on("receive",
     -- upvals: now, ip, txts, _tmr, _best, _kod, _pbest, _res, _six
     function(skt, d, port, rxip)
      -- many things constitute bad packets; drop with tmr running
      if rxip ~= ip and ip ~= "224.0.1.1" then return end   -- wrong peer (unless multicast)
      if port ~= 123 then return end                        -- wrong port
      if #d   <   48 then return end                        -- too short

      local pok, pkt = pcall(sntppkt.proc_pkt, d, txts, now())

      if not pok or pkt == nil then
        -- sntppkt can also reject the packet for having a bad cookie;
        -- this is important to prevent processing spurious or delayed responses
        return
      end

      _tmr:unregister()
      skt:on("receive", nil) -- skt == _udp
      skt:on("sent", nil)

      if type(pkt) == "string" then
        if pkt == "DENY" then -- KoD packet

          if _kod and _kod[rxip] then
            -- There was already a strike against this IP address, and now
            -- it's permanent.  We can't directly remove the IP from rotation,
            -- but we can remove the DNS that's resolving to it, which isn't
            -- great, but isn't the worst either.
            if fc then fc("kod", serv[_six], _self) end
            _kod[rxip] = nil
            table.remove(serv, _six)
            _six = _six - 1 -- nextServer will add one
          else
            _kod = _kod or {}
            _kod[rxip] = 2
            if fc then fc("goaway", serv[_six], _self, pkt) end
          end
        else
          if fc then fc("goaway", serv[_six], _self, pkt) end
        end
        return nextServer()
      end

      if _pbest == serv[_six] then
        -- this was our favorite server last time; if we don't have a
        -- result or if we'd rather this one than the result we have...
        if not _res or not pkt:pick(_res, true) then
          _res = pkt
          _best = _pbest
        end
      else
        -- this was not our favorite server; take this result if we have no
        -- other option or if it compares favorably to the one we have, which
        -- might be from our favorite from last pass.
        if not _res or _res:pick(pkt, _pbest == _best) then
          _res = pkt
          _best = serv[_six]
        end
      end

      return nextServer()
     end)

    return _udp:send(123, ip,
      -- '#' == 0x23: version 4, mode 3 (client), no LI
      "#\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
      .. txts)
  end

  -- upvals: _sat, _six, _udp, hail, _self
  function doserver(err)
    if _sat == 2 --[[const param: MAX_SERVER_ATTEMPTS]] then
      if fc then fc(err, serv[_six], _self) end
      return nextServer()
    end
    _sat = _sat + 1

    return _udp:dns(serv[_six], function(skt, ip)
      skt:on("dns", nil) -- skt == _udp
      if ip == nil then return doserver("dns") else return hail(ip) end
    end)
  end

  -- Move on to the next server or finish a pass
  --
  -- upvals: fc, serv, sc, _best, _pbest, _res, _sat, _six
  function nextServer()
    if _six >= #serv then
     if _res then
       _pbest = _best
       local res = _res
       local best = _best
       _stop()
       return sc(res:totable(), best, _self)
     else
       _stop()
       if fc then return fc("all", #serv, _self) else return end
     end
    end

    _six = _six + 1
    _sat = 0
    return doserver()
  end

  -- Poke all the servers and invoke the user's callbacks
  --
  -- upvals: _stop, _udp, _ENV, _tmr, _six, nextServer
  function _self.sync()
    _stop()
    _udp = net.createUDPSocket()
    _tmr = tmr.create()
    _udp:listen() -- on random port
    _six = 0
    nextServer()
  end

  function _self.stop()
    local res, best = _res, _best
    _stop()
    return res and res:totable(), best
  end

  return _self

end

-- A utility function which applies a result to the rtc
local update_rtc = function(res, obj)
  local rate = nil
  if obj.rtc_last ~= nil then
    -- adjust drift compensation.  We have three pieces of information:
    --
    --   our idea of time at rx (res.rx_*),
    --   our idea of time at the last sync (obj.rtc_last.rx_*)
    --   the measured theta now (res.theta_us)
    --
    -- We're going to integrate the theta signal over time and use
    -- that to mediate the rate we set, making this a PI controller,
    -- but we might take big steps if theta gets too bad.
    local ok, err_int
    local raw = res.raw
    ok, rate, err_int = pcall(raw.drift_compensate, raw, obj.rtc_last,
                              obj.rtc_err_int or 0)
    if not ok then
      rate = nil -- don't set the rate this time
      obj.rtc_last = nil -- or next time
    else
      obj.rtc_last = res.raw
      obj.rtc_err_int = err_int
    end
  else
    obj.rtc_last = res.raw
  end

  if rate == nil then
    -- update time (and cut rate, in case it's gotten out of hand)
    local now_s, now_us, now_r = rtctime.get()
    local new_s, new_us = now_s + res.theta_s, now_us + res.theta_us
    if new_us > 1000000 then
      new_s  = new_s  + 1
      new_us = new_us - 1000000
    end
    rtctime.set(new_s, new_us, now_r / 2)
  else
    -- just change the rate
    rtctime.set(nil, nil, rate)
  end

  return rate ~= nil
end

-- Default operation
--
-- upvals: new, update_rtc
local go = function(servs, period, sc, fc)
  local sntpobj = new(servs,
    -- wrap the success callback with a utility function for managing the rtc
    -- and polling frequency
    function(res, serv, self)
      local ok = update_rtc(res, self)

      -- if the rate estimator thinks it has this under control, only poll
      -- the server occasionally.  Otherwise, bother it more frequently,
      -- in a "bursty" way
      if ok and ((self.rtc_burst or 0) == 0)
       then self.tmr:interval(period or 1800000)
            self.rtc_burst = nil
       else self.tmr:interval(30000)
            self.rtc_burst = (ok and self.rtc_burst or 40) - 1
      end

      -- invoke the user's callback
      if sc then return sc(res, serv, self) end
    end,
    fc)

  local t = tmr.create()
  sntpobj.tmr = t
  t:alarm(60000, tmr.ALARM_AUTO, function() collectgarbage() sntpobj.sync() end)
  sntpobj.sync()

  return sntpobj
end

-- from sntppkt
-- luacheck: ignore
local _lfs_strings = "theta_s", "theta_us", "delta", "delta_r", "epsilon_r",
  "leapind", "stratum", "rx_s", "rx_us"

return {
update_rtc = update_rtc,
new        = new,
go         = go,
}
