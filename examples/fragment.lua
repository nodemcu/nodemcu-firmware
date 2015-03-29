pwm.setup(0,500,50) pwm.setup(1,500,50) pwm.setup(2,500,50)
pwm.start(0) pwm.start(1) pwm.start(2)
function led(r,g,b) pwm.setduty(0,g) pwm.setduty(1,b) pwm.setduty(2,r) end
wifi.sta.autoconnect(1)
a=0
tmr.alarm( 1000,1,function() if a==0 then a=1 led(50,50,50) else a=0 led(0,0,0) end end)

sv:on("receive", function(s,c) s:send("<h1> Hello, world.</h1>") print(c) end )


sk=net.createConnection(net.TCP, 0) 
sk:on("receive", function(sck, c) print(c) end )
sk:connect(80,"115.239.210.27") 
sk:send("GET / HTTP/1.1\r\nHost: 115.239.210.27\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")

sk:connect(80,"192.168.0.66") 
sk:send("GET / HTTP/1.1\r\nHost: 192.168.0.66\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")

i2c.setup(0,1,0,i2c.SLOW)
function read_bmp(addr) i2c.start(0) i2c.address(0,119,i2c.RECEIVER) c=i2c.read(0,1) i2c.stop(0) print(string.byte(c)) end 
function read_bmp(addr) i2c.start(0) i2c.address(0,119,i2c.TRANSMITTER) i2c.write(0,addr) i2c.stop(0) i2c.start(0) i2c.address(0,119,i2c.RECEIVER) c=i2c.read(0,2) i2c.stop(0) return c end 

s=net.createServer(net.TCP) s:listen(80,function(c) end)
ss=net.createServer(net.TCP) ss:listen(80,function(c) end)

s=net.createServer(net.TCP) s:listen(80,function(c) c:on("receive",function(s,c) print(c) end) end)

s=net.createServer(net.UDP) 
s:on("receive",function(s,c) print(c) end)
s:listen(5683)

su=net.createConnection(net.UDP) 
su:on("receive",function(su,c) print(c) end) 
su:connect(5683,"192.168.18.101") 
su:send("hello")

mm=node.list()
for k, v in pairs(mm) do print('file:'..k..' len:'..v) end
for k,v in pairs(d) do print("n:"..k..", s:"..v) end


gpio.mode(0,gpio.INT) gpio.trig(0,"down",function(l) print("level="..l) end)

t0 = 0;
function tr0(l)	print(tmr.now() - t0) t0 = tmr.now()
	if l==1 then gpio.trig(0,"down") else gpio.trig(0,"up") end end
gpio.mode(0,gpio.INT)
gpio.trig(0,"down",tr0)


su=net.createConnection(net.UDP) 
su:on("receive",function(su,c) print(c) end) 
su:connect(5001,"114.215.154.114") 
su:send([[{"type":"signin","name":"nodemcu","password":"123456"}]]) 

su:send([[{"type":"signout","name":"nodemcu","password":"123456"}]]) 

su:send([[{"type":"connect","from":"nodemcu","to":"JYP","password":"123456"}]]) 

su:send("hello world")

s=net.createServer(net.TCP) s:listen(8008,function(c) c:on("receive",function(s,c) print(c) pcall(loadstring(c)) end) end)

s=net.createServer(net.TCP) s:listen(8008,function(c) con_std = c function s_output(str) if(con_std~=nil) then con_std:send(str) end end 
	node.output(s_output, 0) c:on("receive",function(c,l) node.input(l) end) c:on("disconnection",function(c) con_std = nil node.output(nil) end) end)

s=net.createServer(net.TCP) 
s:listen(23,function(c) 
	con_std = c 
	function s_output(str) 
		if(con_std~=nil) 
			then con_std:send(str) 
		end 
	end 
	node.output(s_output, 0) 
	c:on("receive",function(c,l) node.input(l) end) 
	c:on("disconnection",function(c) 
		con_std = nil 
		node.output(nil) 
	end) 
end)

srv=net.createServer(net.TCP) srv:listen(80,function(conn) conn:on("receive",function(conn,payload) 
print(node.heap()) door="open" if gpio.read(8)==1 then door="open" else door="closed" end 
conn:send("<h1> Door Sensor. The door is " .. door ..".</h1>") conn:close() end) end)

srv=net.createServer(net.TCP) srv:listen(80,function(conn) conn:on("receive",function(conn,payload) 
print(node.heap()) print(adc.read(0)) door="open" if gpio.read(0)==1 then door="open" else door="closed" end 
conn:send("<h1> Door Sensor. The door is " .. door ..".</h1>") end) conn:on("sent",function(conn) conn:close() end) end)

srv=net.createServer(net.TCP) srv:listen(80,function(conn) 
	conn:on("receive",function(conn,payload) 
		print(node.heap()) 
		door="open" 
		if gpio.read(0)==1 then door="open" else door="closed" end 
		conn:send("<h1> Door Sensor. The door is " .. door ..".</h1>") 
		end) 
	conn:on("sent",function(conn) conn:close() end) 
end)

port = 9999
hostip = "192.168.1.99"
sk=net.createConnection(net.TCP, false) 
sk:on("receive", function(conn, pl) print(pl) end )
sk:connect(port, hostip)


file.remove("init.lua")
file.open("init.lua","w")
file.writeline([[print("Petes Tester 4")]])
file.writeline([[tmr.alarm(5000, 0, function() dofile("thelot.lua") end )]]) 
file.close()

file.remove("thelot.lua")
file.open("thelot.lua","w")
file.writeline([[tmr.stop()]])
file.writeline([[connecttoap = function (ssid,pw)]])
file.writeline([[print(wifi.sta.getip())]])
file.writeline([[wifi.setmode(wifi.STATION)]])
file.writeline([[tmr.delay(1000000)]])
file.writeline([[wifi.sta.config(ssid,pw)]])
file.writeline([[tmr.delay(5000000)]])
file.writeline([[print("Connected to ",ssid," as ",wifi.sta.getip())]])
file.writeline([[end]])
file.writeline([[connecttoap("MyHub","0011223344")]])
file.close()


s=net.createServer(net.UDP) s:listen(5683) s:on("receive",function(s,c) print(c) s:send("echo:"..c) end)
s:on("sent",function(s) print("echo donn") end)

sk=net.createConnection(net.UDP, 0) sk:on("receive", function(sck, c) print(c) end ) sk:connect(8080,"192.168.0.88") 
sk:send("GET / HTTP/1.1\r\nHost: 192.168.0.88\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")

srv=net.createServer(net.TCP, 5) srv:listen(80,function(conn) conn:on("receive",function(conn,payload) 
print(node.heap()) print(adc.read(0)) door="open" if gpio.read(0)==1 then door="open" else door="closed" end 
conn:send("<h1> Door Sensor. The door is " .. door ..".</h1>") end) end)

srv=net.createServer(net.TCP) 
srv:listen(80,function(conn) 
conn:on("receive",function(conn,payload) 
	print(payload) print(node.heap())
	conn:send("<h1> Hello, NodeMcu.</h1>") 
	end) 
conn:on("sent",function(conn) conn:close() end) 
end)


function startServer()
print("WIFI AP connected. Wicon IP:")
print(wifi.sta.getip())
sv=net.createServer(net.TCP,180)
sv:listen(8080,function(conn)
print("Wifi console connected.")

function s_output(str)
if(conn~=nil) then
conn:send(str)
end
end
node.output(s_output,0)

conn:on("receive",function(conn,pl)
node.input(pl)
if (conn==nil) then
print("conn is nil")
end

print("hello")

mycounter=0 srv=net.createServer(net.TCP) srv:listen(80,function(conn) conn:on("receive",function(conn,payload) 
if string.find(payload,"?myarg=") then mycounter=mycounter+1 
m="<br/>Value= " .. string.sub(payload,string.find(payload,"?myarg=")+7,string.find(payload,"HTTP")-2) else m="" end 
conn:send("<h1> Hello, this is Pete's web page.</h1>How are you today.<br/> Count=" .. mycounter .. m .. "Heap=".. node.heap()) 
end) conn:on("sent",function(conn) conn:close() conn = nil end) end) 

srv=net.createServer(net.TCP) srv:listen(80,function(conn) conn:on("receive",function(conn,payload) 
        conn:send("HTTP/1.1 200 OK\r\n") conn:send("Connection: close\r\n\r\n") conn:send("<h1> Hello, NodeMcu.</h1>")
        print(node.heap()) conn:close() end) end)


conn=net.createConnection(net.TCP)
conn:dns("www.nodemcu.com",function(conn,ip) print(ip) print("hell") end)

function connected(conn) conn:on("receive",function(conn,payload) 
        conn:send("HTTP/1.1 200 OK\r\n") conn:send("Connection: close\r\n\r\n") conn:send("<h1> Hello, NodeMcu.</h1>")
        print(node.heap()) conn:close() end) end
srv=net.createServer(net.TCP) 
srv:on("connection",function(conn) conn:on("receive",function(conn,payload) 
        conn:send("HTTP/1.1 200 OK\r\n") conn:send("Connection: close\r\n\r\n") conn:send("<h1> Hello, NodeMcu.</h1>")
        print(node.heap()) conn:close() end) end)
srv:listen(80)


-- sieve.lua
-- the sieve of Eratosthenes programmed with coroutines
-- typical usage: lua -e N=500 sieve.lua | column

-- generate all the numbers from 2 to n
function gen (n)  return coroutine.wrap(function ()  for i=2,n do coroutine.yield(i) end   end) end

-- filter the numbers generated by `g', removing multiples of `p'
function filter (p, g) return coroutine.wrap(function () for n in g do if n%p ~= 0 then coroutine.yield(n) end end end) end

N=N or 500		-- from command line
x = gen(N)		-- generate primes up to N
while 1 do
  local n = x()		-- pick a number until done
  if n == nil then break end
  print(n)		-- must be a prime number
  x = filter(n, x)	-- now remove its multiples
end

file.remove("mylistener.lua")
file.open("mylistener.lua","w")
file.writeline([[gpio2 = 9]])
file.writeline([[gpio0 = 8]])
file.writeline([[gpio.mode(gpio2,gpio.OUTPUT)]])
file.writeline([[gpio.write(gpio2,gpio.LOW)]])
file.writeline([[gpio.mode(gpio0,gpio.OUTPUT)]])
file.writeline([[gpio.write(gpio0,gpio.LOW)]])
file.writeline([[l1="0\n"]])
file.writeline([[l2="0\n"]])
file.writeline([[l3="0\n"]])
file.writeline([[l4="0\n"]])
file.writeline([[sv=net.createServer(net.TCP, 5) ]]) 
file.writeline([[sv:listen(4000,function(c)]])
file.writeline([[c:on("disconnection", function(c) print("Bye") end )]])
file.writeline([[c:on("receive", function(sck, pl) ]])
-- file.writeline([[print(pl) ]])
file.writeline([[if (pl=="GO1\n") then c:send(l1) ]])
file.writeline([[elseif pl=="GO2\n" then c:send(l2) ]])
file.writeline([[elseif pl=="GO3\n" then c:send(l3) ]])
file.writeline([[elseif pl=="GO4\n" then c:send(l4) ]])
file.writeline([[elseif pl=="YES1\n" then l1="1\n" c:send("OK\n") gpio.write(gpio2,gpio.HIGH) ]])
file.writeline([[elseif pl=="NO1\n" then l1="0\n" c:send("OK\n") gpio.write(gpio2,gpio.LOW) ]])
file.writeline([[elseif pl=="YES2\n" then l2="1\n" c:send("OK\n") gpio.write(gpio0,gpio.HIGH) ]])
file.writeline([[elseif pl=="NO2\n" then l2="0\n" c:send("OK\n") gpio.write(gpio0,gpio.LOW) ]])
file.writeline([[elseif pl=="YES3\n" then l3="1\n" c:send("OK\n") print(node.heap()) ]])
file.writeline([[elseif pl=="NO3\n" then l3="0\n" c:send("OK\n") print(node.heap()) ]])
file.writeline([[elseif pl=="YES4\n" then l4="1\n" c:send("OK\n") print(node.heap()) ]])
file.writeline([[elseif pl=="NO4\n" then l4="0\n" c:send("OK\n") print(node.heap()) ]])
file.writeline([[else c:send("0\n")	print(node.heap()) ]])	
file.writeline([[end]])
file.writeline([[end)]])
file.writeline([[end)]])
file.close()

file.remove("myli.lua") file.open("myli.lua","w")
file.writeline([[sv=net.createServer(net.TCP, 5) ]]) 
file.writeline([[sv:listen(4000,function(c)]])
file.writeline([[c:on("disconnection", function(c) print("Bye") end )]])
--file.writeline([[c:on("sent", function(c) c:close() end )]])
file.writeline([[c:on("receive", function(sck, pl) ]])
file.writeline([[sck:send("0\n")	print(node.heap()) ]])	
file.writeline([[end)]]) file.writeline([[end)]]) file.close()

sv=net.createServer(net.TCP, 50) sv:listen(4000,function(c) c:on("disconnection",function(c) print("Bye") end)
	c:on("receive", function(sck, pl) sck:send("0\n")	print(node.heap()) end)	end)

sv=net.createServer(net.TCP, 5) sv:listen(4000,function(c) c:on("disconnection",function(c) print("Bye") end)
	c:on("receive", function(sck, pl) sck:send("0\n")	print(node.heap()) end)	c:on("sent", function(sck) sck:close() end) end)

s=net.createServer(net.UDP) 
s:on("receive",function(s,c) print(c) end) 
s:listen(8888)

print("This is a long long long line to test the memory limit of nodemcu firmware\n")
collectgarbage("setmemlimit",8)
print(collectgarbage("getmemlimit"))

tmr.alarm(1,5000,1,function() print("alarm 1") end)
tmr.stop(1)

tmr.alarm(0,1000,1,function() print("alarm 0") end)
tmr.stop(0)

tmr.alarm(2,2000,1,function() print("alarm 2") end)
tmr.stop(2)

tmr.alarm(6,2000,1,function() print("alarm 6") end)
tmr.stop(6)

for k,v in pairs(_G.package.loaded) do print(k) end
for k,v in pairs(_G) do print(k) end
for k,v in pairs(d) do print("n:"..k..", s:"..v) end

a="pin=9"
t={}
for k, v in string.gmatch(a, "(%w+)=(%w+)") do t[k]=v  end
print(t["pin"])

function switch() gpio.mode(4,gpio.OUTPUT) gpio.mode(5,gpio.OUTPUT) tmr.delay(1000000) print("hello world") end
tmr.alarm(0,10000,0,function () uart.setup(0,9600,8,0,1) end) switch()

sk=net.createConnection(net.TCP, 0) sk:on("receive", function(sck, c) print(c) end ) sk:connect(80,"www.nodemcu.com") sk:send("GET / HTTP/1.1\r\nHost: www.nodemcu.com\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")

sk=net.createConnection(net.TCP, 0) sk:on("receive", function(sck, c) print(c) end ) 
sk:on("connection", function(sck) sck:send("GET / HTTP/1.1\r\nHost: www.nodemcu.com\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n") end ) sk:connect(80,"www.nodemcu.com") 


sk=net.createConnection(net.TCP, 0) sk:on("receive", function(sck, c) print(c) end ) sk:connect(80,"115.239.210.27") 
sk:send("GET / HTTP/1.1\r\nHost: 115.239.210.27\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")

sk=net.createConnection(net.TCP, 1) sk:on("receive", function(sck, c) print(c) end ) 
sk:on("connection", function(sck) sck:send("GET / HTTPS/1.1\r\nHost: www.google.com.hk\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n") end ) sk:connect(443,"173.194.72.199") 

wifi.sta.setip({ip="192.168.18.119",netmask="255.255.255.0",gateway="192.168.18.1"})

uart.on("data","\r",function(input) if input=="quit\r" then uart.on("data") else print(input) end end, 0)
uart.on("data","\n",function(input) if input=="quit\n" then uart.on("data") else print(input) end end, 0)
uart.on("data", 5 ,function(input) if input=="quit\r" then uart.on("data") else print(input) end end, 0)
uart.on("data", 0 ,function(input) if input=="q" then uart.on("data") else print(input) end end, 0)

uart.on("data","\r",function(input) if input=="quit" then uart.on("data") else print(input) end end, 1)

for k, v in pairs(file.list()) do print('file:'..k..' len:'..v) end

m=mqtt.Client()
m:connect("192.168.18.101",1883)
m:subscribe("/topic",0,function(m) print("sub done") end)
m:on("message",function(m,t,pl) print(t..":") if pl~=nil then print(pl) end end )
m:publish("/topic","hello",0,0)

uart.setup(0,9600,8,0,1,0)
sv=net.createServer(net.TCP, 60)
global_c = nil
sv:listen(9999, function(c)
	if global_c~=nil then
		global_c:close()
	end
	global_c=c
	c:on("receive",function(sck,pl)	uart.write(0,pl) end)
end)

uart.on("data",4, function(data)
	if global_c~=nil then
		global_c:send(data)
	end
end, 0)

file.open("hello.lua","w+")
file.writeline([[print("hello nodemcu")]])
file.writeline([[print(node.heap())]])
file.close()

node.compile("hello.lua")
dofile("hello.lua")
dofile("hello.lc")

-- use copper addon for firefox
cs=coap.Server()
cs:listen(5683)

myvar=1
cs:var("myvar") -- get coap://192.168.18.103:5683/v1/v/myvar will return the value of myvar: 1

-- function should tack one string, return one string.
function myfun(payload)
	print("myfun called")
	respond = "hello"
	return respond
end
cs:func("myfun") -- post coap://192.168.18.103:5683/v1/f/myfun will call myfun

cc = coap.Client()
cc:get(coap.CON, "coap://192.168.18.100:5683/.well-known/core")
cc:post(coap.NON, "coap://192.168.18.100:5683/", "Hello")


file.open("test1.txt", "a+") for i = 1, 100*1000 do file.write("x") end file.close() print("Done.")
for n,s in pairs(file.list()) do print(n.." size: "..s) end 
file.remove("test1.txt")
for n,s in pairs(file.list()) do print(n.." size: "..s) end
file.open("test2.txt", "a+") for i = 1, 1*1000 do file.write("x") end file.close() print("Done.")


function TestDNSLeak()
     c=net.createConnection(net.TCP, 0)
     c:connect(80, "bad-name.tlddfdf")
     tmr.alarm(1, 3000, 0, function() print("hack socket close, MEM: "..node.heap()) c:close() end) -- socket timeout hack
     print("MEM: "..node.heap())
end
