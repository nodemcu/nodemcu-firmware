local N = require('NTest')("tmr")

N.testasync('SINGLE alarm', function(next)
  local t = tmr.create();
  local count = 0
  t:alarm(200, tmr.ALARM_SINGLE,
    function()
      count = count + 1
      ok(count <= 1, "only 1 invocation")
      next()
    end)

  ok(true, "sync end")
end)

N.testasync('SEMI alarm', function(next)
  local t = tmr.create();
  local count = 0
  t:alarm(200, tmr.ALARM_SEMI,
    function(tp)
      count = count + 1
      if count <= 1 then
        tp:start()
        return
      end
      ok(eq(count, 2), "only 2 invocations")
      next()
    end)
  ok(true, "sync end")
end)

N.testasync('AUTO alarm', function(next)
  local t = tmr.create();
  local count = 0
  t:alarm(200, tmr.ALARM_AUTO,
    function(tp)
      count = count + 1
      if count == 2 then
        tp:stop()
        return next()
      end
      ok(count < 2, "only 2 invocations")
    end)
  ok(true, "sync end")
end)

N.testco('SINGLE alarm coroutine', function(getCB, waitCB)
  local t = tmr.create();
  t:alarm(200, tmr.ALARM_SINGLE, getCB("timer"))

  local name, timer = waitCB()
  ok(eq("timer", name), "CB name matches")
  ok(eq(t, timer), "CB tmr instance matches")

  ok(true, "coroutine end")
end)

N.testco('SEMI alarm coroutine', function(getCB, waitCB)
  local t = tmr.create();
  t:alarm(200, tmr.ALARM_SEMI, getCB("timer"))

  local name, timer = waitCB()
  ok(eq("timer", name), "CB name matches")
  ok(eq(t, timer), "CB tmr instance matches")

  timer:start()

  name, timer = waitCB()
  ok(eq("timer", name), "CB name matches again")
  ok(eq(t, timer), "CB tmr instance matches again")

  ok(true, "coroutine end")
end)

N.testco('AUTO alarm coroutine', function(getCB, waitCB)
  local t = tmr.create();
  t:alarm(200, tmr.ALARM_AUTO, getCB("timer"))

  local name, timer = waitCB()
  ok(eq("timer", name), "CB name matches")
  ok(eq(t, timer), "CB tmr instance matches")

  name, timer = waitCB()
  ok(eq("timer", name), "CB name matches again")
  ok(eq(t, timer), "CB tmr instance matches again")

  timer:stop()

  ok(true, "coroutine end")
end)

N.testco('softwd set positive and negativ values', function(getCB, waitCB)
  tmr.softwd(22)
  tmr.softwd(0)
  tmr.softwd(-1)  -- disable it again
  tmr.softwd(-22) -- disable it again
end)

