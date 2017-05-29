# McuNode
####  一个在线IDE，基于互联网的nodemcu的web代理，在线升级lua，在互联网上使用nodemcu的lua终端，在互联网上访问nodemcu的网页（集成web框架）
MCUNODE连接此服务端的8001

浏览器打开 http://127.0.0.1:<80/8080>/ 或者对应的公网ip

#### NODEMCU推荐使用https://github.com/IoTServ/McuNode-Lib 连接，该库集成了两种功能在一起并存
#####  当然你也可以使用分开的程序来定制自己的程序，https://github.com/IoTServ/McuNode 

####  树莓派请使用LinuxArm

##McuNode简介：
###  总览：
####  McuNode是一个主要针对于NodeMcu的服务端（包含在NodeMcu上运行的支持库），使用者可以运行自己的服务端为自己nodemcu提供云支持，当然你也可以将此服务端提供给他人使用，当前服务端由go语言编写理论上一般单核服务器可以支持多达几万的nodemcu同时连接提供服务。本项目主要支持如下功能：
* 提供在线IDE功能（IDE提供一个基于web的云串口功能，完全实现原本的串口的lua解释器的访问功能；并且提供远程Lua程序升级功能实现基于互联网的完全的控制能力）
![image](https://github.com/IoTServ/McuNode-server/blob/master/imgs/mcunode.png?raw=true)
![image](https://github.com/IoTServ/McuNode-server/blob/master/imgs/download.png?raw=true)
* 提供一个类似于Nginx的web代理功能，它可以通过一个可以通过互联网访问的服务器访问nodemcu提供的网页，以此nodemcu编写的web server不仅可以在局域网可以访问，更重要的是McuNode-Lib还提供了一个很棒的文本框架帮助你完成这项工作，包括参数获取和模板文件的渲染等
![image](https://github.com/IoTServ/McuNode-server/blob/master/imgs/webProxy.png?raw=true)

###  使用方法：
* 你可以使用我们提供的库（https://github.com/IoTServ/McuNode-Lib	通过简单的几行代码就可以体验大部分功能：
~~~Lua
local mcunode = require "mcunode"
mcunode.connect("<id>","<server-ip>","<ssid>","<wifi-password>")  --自定义ID和使用的服务器地址（域名或ip），然后就是连接的热点信息
~~~
复杂一些的例子：
~~~Lua
local mcunode = require "mcunode"

--url:http://mcunodeserver-ip/proxy/<your-id>/gpio?pin=1&val=0
--return:status=success
mcunode.handle("/gpio",function(req,res)
	local pin = req.getParam("pin")
	local val = req.getParam("val")
	if( pin ~= nil) and (val ~= nil) then
		print("pin: "..pin .." , val: " ..val)
		gpio.mode(pin, gpio.OUTPUT)
		gpio.write(pin, val)
		res.setAttribute("status","success")
	end
	return res
end)
mcunode.handle("/test",function(req,res)
	local name = req.getParam("name")
	res.setAttribute("name",name)
	res.body="wodename:{{name}}"
	return res
end)
--url:http://mcunodeserver-ip/proxy/<your-id>/index.html?name=farry&work=student
mcunode.handle("/index.html",function(req,res)
  local name = req.getParam("name")
  res.setAttribute("name",name)
  local work = req.getParam("work")
  res.setAttribute("work",work)
  res.file = "indextpl.html" -- ... <h1>{{name}}</h1> ...
  return res
end)
mcunode.connect("<id>","<server-ip>","<ssid>","<wifi-password>")
~~~
以上的复杂一些的例子需要一个模板文件indextpl.html，内容如下：
~~~Html
<h1>HELLO Farry</h1>
{{name}}:{{work}}
<h1>HELLO Farry</h1>
<h1>HELLO Farry</h1>
<h1>HELLO Farry</h1>
<h1>HELLO Farry</h1>
<h1>HELLO Farry</h1>
~~~
这个模板会被注册为/index.html的地址所使用，持续中先从URL中获取两个参数然后将两个参数又渲染到模板反馈给用户
####  而且以上两个程序已经同时支持了在线IDE的功能

* 当然如果你觉得有些功能用不上或者库太大，你也可以从这里：https://github.com/IoTServ/McuNode-server/tree/master/NODEMCU%E7%94%A8Lua%E6%B5%8B%E8%AF%95%E7%94%A8%E4%BE%8B	找到一些有用的信息定制自己的代码
### 非常希望 有人能提供运行本服务端的实例给大家使用，如果有请与我联系，我将在这里提供相关信息
