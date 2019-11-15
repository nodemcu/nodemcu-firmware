-- $Id: api.lua,v 1.147 2016/11/07 13:06:25 roberto Exp $
-- See Copyright Notice in file all.lua

if T==nil then
  (Message or print)('\n >>> testC not active: skipping API tests <<<\n')
  return
end

local debug = require "debug"

local pack = table.pack


function tcheck (t1, t2)
  assert(t1.n == (t2.n or #t2) + 1)
  for i = 2, t1.n do assert(t1[i] == t2[i - 1]) end
end


local function checkerr (msg, f, ...)
  local stat, err = pcall(f, ...)
  assert(not stat and string.find(err, msg))
end


print('testing C API')

--[=[
do
  local f = T.makeCfunc[[
    getglobal error
    pushstring bola
    pcall 1 1 1   # call 'error' with given handler
    pushstatus
    return 2     # return error message and status
  ]]

  local msg, st = f({})     -- invalid handler
  assert(st == "ERRERR" and string.find(msg, "error handling"))
  local msg, st = f(nil)     -- invalid handler
  assert(st == "ERRERR" and string.find(msg, "error handling"))

  local a = setmetatable({}, {__call = function (_, x) return x:upper() end})
  local msg, st = f(a)   -- callable handler
  assert(st == "ERRRUN" and msg == "BOLA")
end

do  -- test returning more results than fit in the caller stack
  local a = {}
  for i=1,1000 do a[i] = true end; a[999] = 10
  local b = T.testC([[pcall 1 -1 0; pop 1; tostring -1; return 1]],
                    table.unpack, a)
  assert(b == "10")
end


-- testing globals
_G.a = 14; _G.b = "a31"
local a = {T.testC[[
  getglobal a;
  getglobal b;
  getglobal b;
  setglobal a;
  return *
]]}
assert(a[2] == 14 and a[3] == "a31" and a[4] == nil and _G.a == "a31")


-- colect in cl the `val' of all collected userdata
tt = {}
cl = {n=0}
A = nil; B = nil
local F
F = function (x)
  local udval = T.udataval(x)
  table.insert(cl, udval)
  local d = T.newuserdata(100)   -- cria lixo
  d = nil
  assert(debug.getmetatable(x).__gc == F)
  assert(load("table.insert({}, {})"))()   -- cria mais lixo
  collectgarbage()   -- forca coleta de lixo durante coleta!
  assert(debug.getmetatable(x).__gc == F)   -- coleta anterior nao melou isso?
  local dummy = {}    -- cria lixo durante coleta
  if A ~= nil then
    assert(type(A) == "userdata")
    assert(T.udataval(A) == B)
    debug.getmetatable(A)    -- just acess it
  end
  A = x   -- ressucita userdata
  B = udval
  return 1,2,3
end
tt.__gc = F

-- test whether udate collection frees memory in the right time
do
  collectgarbage();
  collectgarbage();
  local x = collectgarbage("count");
  local a = T.newuserdata(5001)
  assert(T.testC("objsize 2; return 1", a) == 5001)
  assert(collectgarbage("count") >= x+4)
  a = nil
  collectgarbage();
  assert(collectgarbage("count") <= x+1)
  -- udata without finalizer
  x = collectgarbage("count")
  collectgarbage("stop")
  for i=1,1000 do T.newuserdata(0) end
  assert(collectgarbage("count") > x+10)
  collectgarbage()
  assert(collectgarbage("count") <= x+1)
  -- udata with finalizer
  collectgarbage()
  x = collectgarbage("count")
  collectgarbage("stop")
  a = {__gc = function () end}
  for i=1,1000 do debug.setmetatable(T.newuserdata(0), a) end
  assert(collectgarbage("count") >= x+10)
  collectgarbage()  -- this collection only calls TM, without freeing memory
  assert(collectgarbage("count") >= x+10)
  collectgarbage()  -- now frees memory
  assert(collectgarbage("count") <= x+1)
  collectgarbage("restart")
end


collectgarbage("stop")

-- create 3 userdatas with tag `tt'
a = T.newuserdata(0); debug.setmetatable(a, tt); na = T.udataval(a)
b = T.newuserdata(0); debug.setmetatable(b, tt); nb = T.udataval(b)
c = T.newuserdata(0); debug.setmetatable(c, tt); nc = T.udataval(c)

-- create userdata without meta table
x = T.newuserdata(4)
y = T.newuserdata(0)

-- checkerr("FILE%* expected, got userdata", io.input, a)
-- checkerr("FILE%* expected, got userdata", io.input, x)

assert(debug.getmetatable(x) == nil and debug.getmetatable(y) == nil)

d=T.ref(a);
e=T.ref(b);
f=T.ref(c);
t = {T.getref(d), T.getref(e), T.getref(f)}
assert(t[1] == a and t[2] == b and t[3] == c)

t=nil; a=nil; c=nil;
T.unref(e); T.unref(f)

collectgarbage()

]=]

-------------------------------------------------------------------------
-- testing memory limits
-------------------------------------------------------------------------
T.totalmem(T.totalmem()+45*2048)   -- set low memory limit
local function fillstack(x,a,b,c,d,e,f,g,h,i)
  local j,k,l,m,n,o,p,q,r = a,b,c,d,e,f,g,h,i
  if x > 0 then fillstack(x-1,j,k,l,m,n,o,p,q,r) end
end

local calls = 1
local function gcfunc(ud) -- upval: calls
  local j,k,l,m,n,o,p,q,r = 0,0,0,0,0,0,0,0,0
  print (('GC for %s called with %u fillstacks'):format(tostring(ud),calls))
  calls = calls + 10
  fillstack(calls,j,k,l,m,n,o,p,q,r)
end
local a ={}
collectgarbage("restart")
collectgarbage("setpause",120)
collectgarbage("setstepmul",1000)
for i = 1, 20 do
  local UD = T.newuserdata(2048); debug.setmetatable(UD, {__gc = gcfunc})
  a[i] = UD
  print (('a[%u] = %s'):format(i, tostring(UD)))
end

for i = 1, 20 do
  j = (513 * i % 20) + 1
  k = (13 + 257 * i % 20) + 1
  calls = calls + 1
  local UD = T.newuserdata(2048); debug.setmetatable(UD, {__gc = gcfunc})
  print (('setting a[%u] = nil, a[%u] = %s'):format(j, k, tostring(UD)))
  a[j]=nil; a[k] = UD
end
print ("Done")
--[=[
checkerr("block too big", T.newuserdata, math.maxinteger)
collectgarbage()
T.totalmem(T.totalmem()+5000)   -- set low memory limit (+5k)
checkerr("not enough memory", load"local a={}; for i=1,100000 do a[i]=i end")
T.totalmem(0)          -- restore high limit

-- test memory errors; increase memory limit in small steps, so that
-- we get memory errors in different parts of a given task, up to there
-- is enough memory to complete the task without errors
function testamem (s, f)
  collectgarbage(); collectgarbage()
  local M = T.totalmem()
  local oldM = M
  local a,b = nil
  while 1 do
    M = M+7   -- increase memory limit in small steps
    T.totalmem(M)
    a, b = pcall(f)
    T.totalmem(0)  -- restore high limit
    if a and b then break end       -- stop when no more errors
    collectgarbage()
    if not a and not    -- `real' error?
      (string.find(b, "memory") or string.find(b, "overflow")) then
      error(b, 0)   -- propagate it
    end
  end
  print("\nlimit for " .. s .. ": " .. M-oldM)
  return b
end


-- testing memory errors when creating a new state

b = testamem("state creation", T.newstate)
T.closestate(b);  -- close new state

-- testing luaL_newmetatable
local mt_xuxu, res, top = T.testC("newmetatable xuxu; gettop; return 3")
assert(type(mt_xuxu) == "table" and res and top == 3)
local d, res, top = T.testC("newmetatable xuxu; gettop; return 3")
assert(mt_xuxu == d and not res and top == 3)
d, res, top = T.testC("newmetatable xuxu1; gettop; return 3")
assert(mt_xuxu ~= d and res and top == 3)

x = T.newuserdata(0);
y = T.newuserdata(0);
T.testC("pushstring xuxu; gettable R; setmetatable 2", x)
assert(getmetatable(x) == mt_xuxu)

-- testing luaL_testudata
-- correct metatable
local res1, res2, top = T.testC([[testudata -1 xuxu
   	 			  testudata 2 xuxu
				  gettop
				  return 3]], x)
assert(res1 and res2 and top == 4)

-- wrong metatable
res1, res2, top = T.testC([[testudata -1 xuxu1
			    testudata 2 xuxu1
			    gettop
			    return 3]], x)
assert(not res1 and not res2 and top == 4)

-- non-existent type
res1, res2, top = T.testC([[testudata -1 xuxu2
			    testudata 2 xuxu2
			    gettop
			    return 3]], x)
assert(not res1 and not res2 and top == 4)

-- userdata has no metatable
res1, res2, top = T.testC([[testudata -1 xuxu
			    testudata 2 xuxu
			    gettop
			    return 3]], y)
assert(not res1 and not res2 and top == 4)

-- erase metatables
do
  local r = debug.getregistry()
  assert(r.xuxu == mt_xuxu and r.xuxu1 == d)
  r.xuxu = nil; r.xuxu1 = nil
end
]=]
print'OK'

