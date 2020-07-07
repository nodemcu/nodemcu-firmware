local test = require('gambiarra')

-- set the output handler
local function TERMINAL_HANDLER(e, test, msg, errormsg)
  if errormsg then
    errormsg = ": "..errormsg
  else
    errormsg = ""
  end
  if e == 'pass' then
    print("  "..e.." "..test..': '..msg)
  elseif e == 'fail' then
    print(" ==>  "..e.." "..test..': '..msg..errormsg)
  elseif e == 'except' then
    print(" ==>  "..e.." "..test..': '..msg..errormsg)
  else
    print(e.." "..test)
  end
end
-- set the output handler
test(TERMINAL_HANDLER)

test('SINGLE alarm', function(next)
  local t = tmr.create();
  local count = 0
  t:alarm(200, tmr.ALARM_SINGLE,
    function(t)
      count = count + 1
      ok(count <= 1, "only 1 invocation")
      next()
    end)

  ok(true, "sync end")
end, true)

test('SEMI alarm', function(next)
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
end, true)

test('AUTO alarm', function(next)
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
end, true)

