--[[  A coroutine Helper   T. Ellison,  June 2019

This version of couroutine helper demonstrates the use of corouting within
NodeMCU execution to split structured Lua code into smaller tasks

]]
--luacheck: read globals node

local modname = ...

local function taskYieldFactory(co)
  local post = node.task.post
  return function(nCBs)  -- upval: co,post
    post(function () -- upval: co, nCBs
      coroutine.resume(co, nCBs or 0)
    end)
    return coroutine.yield() + 1
  end
end

return { exec = function(func, ...) -- upval: modname
  package.loaded[modname] = nil
  local co = coroutine.create(func)
  return coroutine.resume(co, taskYieldFactory(co), ... )
end }


