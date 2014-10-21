nodeMcu API说明
=======
version 0.1 2014-10-11

flash 错误
----
		注意：有些模块在烧写之后启动，串口输出 ERROR in flash_read: r=。。。
		这是因为模块原来的flash内部没有擦除。
		可使用blank.bin(4k)，重复烧入其实地址0x47000, 0x48000, 0x49000, 0x4a000扇区.
		或者自行生成一个512k大小的blank512k.bin, 内容为全0xFF，从0x00000开始烧入flash。
		烧入之后可以正常运行。

概述
------
		nodeMcu 支持一键配置

		nodeMcu 基于Lua 5.1.4，使用者需了解最简单的Lua语法。

		nodeMcu 尽量采用事件驱动的编程模型。

		nodeMcu 内置 timer，pwm，i2c，net，gpio，wifi module。

		nodeMcu对模块的引脚进行编号，gpio，i2c，pwm等模块需要使用引脚编号进行索引。

###目前的编号对应表格：

		IO索引编号	ESP8266实际IO	IO索引编号	ESP8266实际IO
		0			GPIO12			8			GPIO0
		1			GPIO13			9			GPIO2
		2			GPIO14			10			GPIO4
		3			GPIO15			11			GPIO5
		4			GPIO3		
		5			GPIO1		
		6			GPIO9		
		7			GPIO10		

###串口波特率
9600

###固件烧写地址：

		eagle.app.v6.flash.bin：0x00000
		eagle.app.v6.irom0text.bin：0x10000
		esp_init_data_default.bin：0x7c000
		blank.bin：0x7e000

		第一次使用建议将blank512k.bin烧写在地址0x00000

node module
------
node.restart()

		描述：重新启动

node.dsleep( us )

		描述：深度睡眠 us 微秒，时间到之后将重新启动
		us：时间，单位微秒

node.chipid() 

		描述：返回芯片id
		返回：number

node.heap()

		描述：返回可用内存 in bytes
		返回：系统heap剩余

node.format()

		描述：格式化用户flash区

node.startlog( filename, noparse)

		描述：开始记录输入
		filename：log所保存的文件名，不支持目录
		noparse：1表示lua虚拟机不对输入进行解析，0表示lua虚拟机对输入进行解析

node.stoplog()

		描述：结束log

例子：录制log到init.lua文件，可以在系统启动之后自动调用该文件。

		node.format()
		node.startlog("init.lua", 1)
		print("hello world")
		node.stoplog()

此时，文件init.lua内部将含有内容，重启之后，系统执行print("hello world")

node.readlog( filename) 

		描述：读取文件
		filename：log文件名，不支持目录
		返回：读取的内容，字符串形式

node.list()

		描述：返回所有文件
		返回：一个包含 <文件名：文件大小> pair 的map

wifi module
-----------

常量：wifi.STATION, wifi.SOFTAP, wifi.STATIONAP

wifi.setmode(mode)

		描述：设置wifi的工作模式
		mode：取值：wifi.STATION, wifi.SOFTAP或wifi.STATIONAP
		返回设置之后的当前mode

wifi.getmode()

		描述：获取wifi的工作模式
		返回：wifi.STATION, wifi.SOFTAP或wifi.STATIONAP

wifi.startconfig( channel, function succeed_callback )

		描述：开始智能配置，若成功自动设置ssid和pwd并退出
		channel：1~13，起始搜索信道。若不指定，默认为6，每个信道搜索20秒
		succeed_callback：配置成功后的回调函数，将在获取密码正确并连接上ap之后调用

wifi.stopconfig()

		描述：中断智能配置

wifi.station module
-----------------
wifi.station.setconfig(ssid, password)

		描述：设置station模式下的ssid和password
		ssid：字符串形式，长度小于32
		password：字符串形式，长度小于64

wifi.station.connect()

		描述：station模式下连接至ap

wifi.station.disconnect()

		描述：station模式下断开与ap的连接

wifi.station.autoconnect(auto)

		描述：station模式下设置自动连接至ap
		auto：0表示设置为不自动连，1表示设置为自动连接

wifi.station.getip()

		描述：station模式下获取ip
		返回：字符串形式的ip，如"192.168.0.2"

wifi.station.getmac()

		描述：station模式下获取mac
		返回：字符串形式的mac，如"18-33-44-FE-55-BB"

wifi.ap module
---------------
wifi.ap.setconfig(cfg)

		描述：设置station模式下的ssid和password
		cfg：设置需要的map

例子：

		cfg={}
		cfg.ssid="myssid"
		cfg.pwd="mypwd"
		wifi.ap.setconfig(cfg)

wifi.ap.getip()

		描述：ap模式下获取ip
		返回：字符串形式的ip，如"192.168.0.2"

wifi.ap.getmac()

		描述：ap模式下获取mac
		返回：字符串形式的mac，如"1A-33-44-FE-55-BB"

gpio module
-----------
常量：gpio.OUTPUT, gpio.INPUT, gpio.HIGH, gpio.LOW

gpio.mode( pin, mode) 

		描述：设置对应pin的输入输出模式，将该pin初始化为gpio模式。
		pin：0~11，IO索引编号
		mode：gpio.OUTPUT或者gpio.INPUT

gpio.read(pin)

		描述：读取对应pin的值
		pin：0~11，IO索引编号
		返回：0表示低，1表示高

gpio.write(pin, level)

		描述：设置对应pin的值
		pin：0~11，IO索引编号
		level：gpio.HIGH或者gpio.LOW

例子：

		pin=1
		gpio.mode(pin, gpio.OUTPUT)
		gpio.write(pin, gpio.HIGH)

将索引1的pin设置为GPIO模式，并设置为高电平。

net module
---------------
常量：net.TCP, net.UDP

net.createServer(type)

		描述：建立一个服务器
		type：net.TCP或 net.UDP
		返回：net.server子模块

net.createConnection(type)

		描述：建立一个客户端
		type：net.TCP或 net.UDP
		返回：net.socket子模块

net.server module
-------------
listen(port,[ip],function(net.socket))

		描述：监听某端口
		port：端口号
		ip：可忽略，ip字符串
		function(net.socket): 回调函数，当有连接建立的时候，作为参数传给回调函数。

例子：

		sv=net.createServer(net.TCP, false)
		sv:listen(80,function(c) 
			c:on("receive", function(sck, pl) print(pl) end)
			c:send("hello world") 
			end)

close()

		描述：关闭服务器

net.socket module
-------
connect(port, ip)

		描述：连接到某ip和端口
		port：端口号
		ip：ip字符串

send( string, function(sent) )

		描述：向连接发送数据
		string：需要发送的数据字符串

on(event, function cb())

		描述：注册事件的回调函数
		event：字符串，可为："connection"，"reconnection"，"disconnection"，"receive"，"sent"
		function cb(net.socket, [string])：回调函数。第一个参数为socket连接本身。
		若event为"receive"， 第二个参数为接收到数据，字符串形式。

例子：

		sk=net.createConnection(net.TCP, false)
		sk:on("receive", function(sck, pl) print(pl) end )
		sk:connect(80,"115.239.210.27") 
		sk:send("GET / HTTP/1.1\r\nHost: 115.239.210.27\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")

close()

		描述：关闭socket

dns(domain, function cb(net.socket, ip) )

		描述：获取domain的ip
		domain：字符串
		function cb(net.socket, ip)：回调函数。第一个参数为socket连接本身， 第二个为获取的ip, 字符串形式。
