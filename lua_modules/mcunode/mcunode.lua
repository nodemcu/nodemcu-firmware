local moduleName = "mcunode"
local M = {}
_G[moduleName] = M

local handlers = {}

local files = {}

--plugin
function ls()
	local l = file.list()
	for k,v in pairs(l) do
		print("name:"..k..", size:"..v)
	end
end
function cat(filename)
	local line
	file.open(filename, "r")
	while 1 do
		line = file.readline() 
		if line == nil then
			break
		end
		line = string.gsub(line, "\n", "")
		print(line)
	end
	file.close()
end

local function urlDecode(str)
    if str == nil then
        return nil
    end
    str = string.gsub(str, '+', ' ')
    str = string.gsub(str, '%%(%x%x)', function(h) return string.char(tonumber(h, 16)) end)
    return str
end

-- 解析request请求参数
local function parseRequest(data)
  --print("data"..data)
  local req = {}
  local param = nil
  req.parameter = {}
  req.method = 'GET'
  req.protocol = 'http'
  -- 解析请求路径及请求参数
  _,_,req.path,param = string.find(data,"([^%?]+)%??([^%s]*)")   --1       16      gpio    pin=1&val=0   path ="gpio?pin=1&val=0"
  --print("param"..param)
  if param ~= nil then --解析请求参数
    --print(param.."not nil")
    for key,value in string.gmatch(param,"([^%?%&]+)=([^%?%&]+)") do
	  --print(key..value)
      req.parameter[key] = urlDecode(value)  --pin 1 , val 0   param ="gpio?pin=1&val=0"
    end
    param = nil
  end
  --print("request path : " .. req.path)
  data = nil
  req.getParam = function(name) --获取请求参数
	if(name == nil) then
		return req.parameter
	end
    return req.parameter[name]  --"farry"
  end
  return req
end



-- tabel to string
local function renderString(subs)
  local content = ""
  for k,v in pairs(subs) do
	content = content.. k .."=" .. v ..","
  end
  if ( #content == 0 ) then
	return nil
  end
  return string.sub(content,1,#content -1)
end

-- 渲染返回数据
local function render(conn,res)
	local body = nil
	local attr = res.attribute
	html=""
	if res.file then
		--print("file")
        file.open(res.file,"r")
		--print("response file : " .. res.file)
		while true do
			local line = file.readline()
			if line == nil then
				break
			end
			if attr then
				for k, v in pairs(attr) do
					line= string.gsub(line, '{{'..k..'}}', v)
				end
			end
			--html=html..line
			conn:send(line)
			line=""
		end
		file.close()
	elseif res.body  then
		--print("body")
		body = res.body
		if attr then
		  for k, v in pairs(attr) do
			body = string.gsub(body, '{{'..k..'}}', v)
		  end
		end
		--print(body)
		html=body
		conn:send(html)
	elseif attr then
		--print("attr")
		body = renderString(attr)
		if body then
			html=body
			conn:send(html)
		end
	end
	--print("send"..html)
	html=nil
	res = nil
	body = nil
	attr = nil
end


local function receive(conn,data)
    local s = tmr.now() -- start time
	local req = parseRequest(data)
	local func = handlers["/"..req.path]
	local response = {}
	response.attribute = {} --当file或body不为nil时，做变量置换，否则body为attribute解析之后的结果
	response.body = nil --响应体
	response.file = nil -- 静态文件
	response.setAttribute = function(k,v)
		if(type(k) == "table") then
			for key,val in pairs(k) do
				response.attribute[key] = val
			end
		else
			response.attribute[k] = v
		end
	end
	response.getAttribute = function(k)
		if(k == nil) then
			return response.attribute
		end
		return response.attribute[k]
	end
	tmr.wdclr()
	if func == nil then -- 没有匹配路径
		node.input(data)
		function s_output(str)
			if (conn~=nil and str~='')    then
				conn:send(str)
			 end
		end
		node.output(s_output,1)
	elseif func == "file" then
		response.file = req.path
	else
		response = func(req,response)
	end
    req = nil
	if response then
		render(conn,response)
	end
	response = nil
	local e = tmr.now() -- end time
	--print("heap:" .. node.heap() .. "bytes, start_time : ".. s .. "us,end_time:".. e .."us,total_time : " .. (e-s) .."us")
	collectgarbage("collect")
	--node.output(nil)
end

function M.handle(path, func)
    handlers[path] = func
end

function M.connect( id , host , ssid , pwd )
	conn = net.createConnection(net.TCP, 0)
	conn:on("connection", function(sk, c)
					sk:send(id)
					tmr.alarm(2, 100000, 1, function() sk:send('<h1></h1>') end)	
				end)
	conn:on('receive', receive)			
	if (ssid~=nil)	then
        wifi.setmode(wifi.STATION)
		wifi.sta.config(ssid,tostring(pwd or ""))    --set your ap info !!!!!!
		wifi.sta.autoconnect(1)
		tmr.alarm(1, 1000, 1, function() 
			if wifi.sta.getip()==nil then
			  print("Connect AP, Waiting...") 
			else
				tmr.stop(1)
				conn:connect(8001,host)
				print("Mcunode Terminal and web proxy is running !\nId is " .. id.." host is "..host)
				local l = file.list()
				for k,v in pairs (l) do
					if not string.find(k,".lc") then
						handlers["/"..k] = "file"
					end
				end
				l = nil
				for k,v in pairs (handlers) do
					print("path:" .. k)
				end
				--collectgarbage("collect")
				--tmr.delay(100000)
				print("localhost ip : "..wifi.sta.getip())
			end	
		end)
	end
end

return M