-- 
local function enum(t,log) for k,v in pairs(t)do log(k,v) end end   
return {entry = function(msg)
    package.loaded["luaOTA.default"]=nil
    local gc=collectgarbage; gc(); gc()
    if DEBUG then 
      for k,v in pairs(_G) do print(k,v)  end 
      for k,v in pairs(debug.getregistry()) do print(k,v) end 
    end
    gc(); gc() 
    print(msg, node.heap())
  end}
