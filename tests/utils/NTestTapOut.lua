-- This is a NTest output handler that formats its output in a way that
-- resembles the Test Anything Protocol (though prefixed with "TAP: " so we can
-- more readily find it in comingled output streams).

local nrun
return function(e, test, msg, err)
  msg = msg or ""
  err = err or ""
  if     e == "pass" then
    print(("\nTAP: ok %d %s # %s"):format(nrun, test, msg))
    nrun = nrun + 1
  elseif e == "fail" then
    print(("\nTAP: not ok %d %s # %s: %s"):format(nrun, test, msg, err))
    nrun = nrun + 1
  elseif e == "except" then
    print(("\nTAP: not ok %d %s # exn; %s: %s"):format(nrun, test, msg, err))
    nrun = nrun + 1
  elseif e == "abort" then
    print(("\nTAP: Bail out! %d %s # exn; %s: %s"):format(nrun, test, msg, err))
  elseif e == "start" then
    -- We don't know how many tests we plan to run, so emit a comment instead
    print(("\nTAP: # STARTUP %s"):format(test))
    nrun = 1
  elseif e == "finish" then
    -- Ah, now, here we go; we know how many tests we ran, so signal completion
    print(("\nTAP: POST 1..%d"):format(nrun))
  elseif #msg ~= 0 or #err ~= 0 then
    print(("\nTAP: # %s: %s: %s"):format(test, msg, err))
  end
end
