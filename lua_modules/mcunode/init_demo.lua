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
mcunode.connect("<define_id>","<your-server-ip>","<wifi_ssid>","<wifi_password>")
