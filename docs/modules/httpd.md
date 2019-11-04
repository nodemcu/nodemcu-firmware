# httpd Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-11-03 |  Nik| Nik | [httpd.c](../../app/modules/httpd.c)|

## About
This is a simple httpd server. This server is not thought for high troughput nor for public access with 100ts of concurrent requests. The HW simple does not allow this.

- Uses only 1 or 2 k of your memory, depending mainly on the size of the http{} table.
- Upload speed is up to 100kByte/s - this depends on the WiFi connetion however
- Simple read requestes (1 frame / up to 1.5k ) are handled 60ms, 2 frams in 75ms
- Permission can be set to read / write / exec (note: this works but need improvement)
- This server is **not 'secure'**, it's plain html with no tls option
- Adding http digest authentification works, but leavs the 'man in the middle attack' problem

## How it works
This server expects in the first packet it receives a complete http request header. It parses this header and returns a table with the request to the receive function.
The on:receive function then has a chance to fullfill the request, or call the procReq() with the request in order to complete it. 
The reason for this detour through the receive function is mainly to leave a way of intercepting the request befor answering.

## Usage

```lua
      if srv then srv:close() end
      srv=net.createServer(net.TCP, 18)
          srv:listen(80, function(socket)
              local R={} -- R will survive as upvalue while until the socket is closed
              socket:on("sent", function(socket) socket:close() end)
              socket:on("receive", function(socket, Req) 
                    if Req['frm']==1 then -- the initial frame with the http request
                       R=Req -- we need the information until we close this socket
                       --print("sock:", socket:getpeer())
                       --print("-- uri="..(Req['uri'] or "nil")..", opts="..(Req['opts'] or "nil")) 
                       --for k,v in pairs(Req) do print(k..", ", v) end        
                    end
                 socket:procReq(R) -- this will process the request
                 end)
             end,
             net.HTTP -- by setting this to net.HTTP, each request will be parsed for a http request header
             ) -- end of listen
```


## Configuration
The configuration of the server is done by simply defining a global lua table named 'http{}' with a minimal content of:
```lua
    http{ ['_any_']='0x0444' }  -- special case, matches all uri (read)
```
## Global Variables
```lua
   http_reqs -- number of http request since start
   http{}      -- table containing the requestable http pages
```

## Permission
The idea behind the permission comes from the http authentification. With set up authentication we can define different users with differen rights.
As this is potentially insecure because of mitm attacks i didn'f finish this. We still can use the server in a secure way by disallowing everything and only allow explicit request.
So the concept behind it is to have kind of unix rights. 4/read, 2/write, 1/exec. from right to left guest, user, admin.
By default a request comes in as guest. The server checks the request for requirements like read, write or execute and verifies this agains the definition in the table:

### example 1

a simple configuration 
```lua
local function some_httpdsamples()
    if http==nil then http={} end
    http['_any_']       ='0x0666' -- special case, matches all uri
    http['cmd']         ='0x0111' -- run 'node.input(cmd)' eg: curl -d "print(node.heap())" "http://$ip/cmd"
    http['up']          ='0x0222' -- upload files
    http['/']           = http['/'] or 'index.html' -- /

    http['iptest']      ='0x0111' -- the 
    http['f_iptest']    =function(s, T) local p,ip=s:getpeer() s:send_H(201,7,0,1,ip) end

    http['heap']        ='0x0111' -- curl -v http://192.168.1.xx/heap
    http['f_heap']      =function(socket, T) socket:send_H(201,7,0,1,'heap:'..node.heap()) end

    http['testfunc']    ='0x0111'
    http['f_testfunc']  =function(socket, T) socket:send_H(201,7,0,1,T['uri']..' opts~'..T['opts']..'~') end
end
```
## Constants
Constants to be used in other functions: `net.RAW`, `net.HTTP`

## net.createServer()

Notes: this function specifies the protocol in addition to the normal server creation. 

#### Syntax
`net.createServer([type[, timeout]], [protocol])`

#### Parameters
- `type` `net.TCP` (default) or `net.UDP`
- `timeout` for a TCP server timeout is 1~28'800 seconds, 30 sec by default (for an inactive client to be disconnected)
- `type` `net.RAW` (default) or `net.HTTP`

!!! attention
    The `type` parameter will be removed in upcoming releases so that `net.createServer` will always create a TCP-based server. For UDP use [net.createUDPSocket()](#netcreateudpsocket) instead.

#### Returns

- for `net.TCP` - net.server sub module
- for `net.UDP` - net.udpsocket sub module

#### Example

```lua
net.createServer(net.TCP, 30, net.HTTP) -- 30s timeout
```

#### See also
[`net.createConnection()`](#netcreateconnection), [`net.createUDPSocket()`](#netcreateudpsocket)


## net.send_H()
Send back a http header. This will send back a http conform response. It will calculate the length if needed. For `content type` = 5 (text/event-stream) a `Connection: Keep-Alive` will be added, all other connetcions will terminate with `Connection: close` after sending.

### Syntax
`net.send_H(status, type, lenght, cache, data)`

### Parameters
- `status code` 100,200,201,302,401,404,408,501,503
  - 100:	HTTP/1.1 100 Continue
  - 200:	HTTP/1.1 200 OK
  - 201: HTTP/1.1 201 Completed
  - 302: HTTP/1.1 302 Found
  - 401: HTTP/1.1 401 Unauthorized
  - 404: HTTP/1.1 404 Not Found
  - 408: HTTP/1.1 408 Request Time-out
  - 501: HTTP/1.1 501 Not Implemented
  - 503: HTTP/1.1 503 Service Unavailable
- `content type`
   - 0: - none -
   - 1: text/html
   - 2: text/plain
   - 3: text/css
   - 4: text/x-lua
   - 5: text/event-stream
   - 6: application/x-javascript
   - 7: application/json
 - `content lenght`
   -  -1 : none
   - else: lenght
- `cache`
   - 1: Cache-control: no-store
   - 2: Cache-control: public, immutable, max-age=31536000
   - 3: Cache-control: public, immutable, max-age=31536000\r\nContent-Encoding: gzip
- `data`

## net.procReq()
This is the generic handler for requests. It will process the request, eg: send and receive http files.
```lua
              socket:on("receive", function(socket, Req) 
                    if Req['frm']==1 then 
                       R=Req  
                    else
                       socket:procReq(R) -- this will process the request
                    end
               end)
```

### Syntax
`net.procReq(R)`

### Parameter
- `R` a local pointer/upvalue to a table with the request data, see further up for a description.




## Testcases 
All testcases are done with the assumption that they are run on a linux pc with 192.168.1.5 on an nodeMcu with ip 192.168.1.204

### GET a simple page
```lua
    http['_any_']       ='0x0666' -- special case, matches all uri
    http['/']           = http['/'] or 'index.html' -- /
```
```bash
curl -v http://192.168.1.204/init.lua
* Hostname was NOT found in DNS cache
*   Trying 192.168.1.204...
  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                 Dload  Upload   Total   Spent    Left  Speed
  0     0    0     0    0     0      0      0 --:--:-- --:--:-- --:--:--     0* 
  Connected to 192.168.1.204 (192.168.1.204) port 80 (#0)
> GET /init.lua HTTP/1.1
> User-Agent: curl/7.35.0
> Host: 192.168.1.204
> Accept: */*
> 
< HTTP/1.1 200 OK
* Server Niks NodeMCU on ESP8266 is not blacklisted
< Server: Niks NodeMCU on ESP8266
< Content-Type: text/x-lua
< Cache-control: no-store
< Connection: close
< 
{ [data not shown]
100  2435    0  2435    0     0  36879      0 --:--:-- --:--:-- --:--:-- 37461
* Closing connection 0

```

### Run a function and return it's result (GET request)
```lua
    http['heap']        ='0x0111' -- curl -v http://192.168.1.xx/heap
    http['f_heap']      =function(socket, T) socket:send_H(201,7,0,1,'heap:'..node.heap()) end
```

```bash
curl -v http://192.168.1.204/heap?
  * Hostname was NOT found in DNS cache
  *   Trying 192.168.1.204...
  * Connected to 192.168.1.204 (192.168.1.204) port 80 (#0)
  > GET /heap? HTTP/1.1
  > User-Agent: curl/7.35.0
  > Host: 192.168.1.204
  > Accept: */*
  > 
  < HTTP/1.1 201 Completed
  * Server Niks NodeMCU on ESP8266 is not blacklisted
  < Server: Niks NodeMCU on ESP8266
  < Content-Type: application/json
  < Cache-control: no-store
  < Connection: close
  < Content-Length: 10
  < 
  * Closing connection 0
heap:34072
```

### Run a function **with options** and return it's result (GET request)
```lua
    http['testfunc']    ='0x0111'
    http['f_testfunc']  =function(socket, T) socket:send_H(201,7,0,1,T['uri']..' opts~'..T['opts']..'~') end
```

```bash
curl -v 'http://192.168.1.204/testfunc?options=11-03-2019'
  * Hostname was NOT found in DNS cache
  *   Trying 192.168.1.204...
  * Connected to 192.168.1.204 (192.168.1.204) port 80 (#0)
  > GET /testfunc?options=11-03-2019 HTTP/1.1
  > User-Agent: curl/7.35.0
  > Host: 192.168.1.204
  > Accept: */*
  > 
  < HTTP/1.1 201 Completed
  * Server Niks NodeMCU on ESP8266 is not blacklisted
  < Server: Niks NodeMCU on ESP8266
  < Content-Type: application/json
  < Cache-control: no-store
  < Connection: close
  < Content-Length: 33
  < 
  * Closing connection 0
testfunc opts~options=11-03-2019
```

### Run a function **with options** (POST request)
This will print on the esp console, only a header is sent back
```lua
    http['cmd']         ='0x0111' -- run 'node.input(cmd)' eg: curl -d "print(node.heap())" "http://$ip/cmd"
```
```bash
echo "print(node.heap( ))" | curl -v -m 1 -d @- 'http://192.168.1.204/cmd'
  * Hostname was NOT found in DNS cache
  *   Trying 192.168.1.204...
  * Connected to 192.168.1.204 (192.168.1.204) port 80 (#0)
  > POST /cmd HTTP/1.1
  > User-Agent: curl/7.35.0
  > Host: 192.168.1.204
  > Accept: */*
  > Content-Length: 19
  > Content-Type: application/x-www-form-urlencoded
  > 
  * upload completely sent off: 19 out of 19 bytes
  < HTTP/1.1 201 Completed
  * Server Niks NodeMCU on ESP8266 is not blacklisted
  < Server: Niks NodeMCU on ESP8266
  < Cache-control: no-store
  < Connection: close
  < 
  * Closing connection 0
```
### Upload a file 
```lua
http['up']          ='0x0222' -- upload files
```
```bash
time curl -H Expect: -v --data-binary @/tmp/50kdummy http://192.168.1.204/up?filen=50kdummy
  * Hostname was NOT found in DNS cache
  *   Trying 192.168.1.204...
  * Connected to 192.168.1.204 (192.168.1.204) port 80 (#0)
  > POST /up?filen=50kdummy HTTP/1.1
  > User-Agent: curl/7.35.0
  > Host: 192.168.1.204
  > Accept: */*
  > Content-Length: 51200
  > Content-Type: application/x-www-form-urlencoded
  > 
  < HTTP/1.1 201 Completed
  * Server Niks NodeMCU on ESP8266 is not blacklisted
  < Server: Niks NodeMCU on ESP8266
  < Cache-control: no-store
  < Connection: close
  < Content-Length: 5
  < 
  * Closing connection 0
51200
real	0m1.158s
user	0m0.000s
sys	0m0.005s
```

### A bit mor complex example, requesting a udp packet from the esp 
```lua
http['cmd']         ='0x0111' -- run 'node.input(cmd)' eg: curl -d "print(node.heap())" "http://$ip/cmd"
```
```bash
nc -u -l 5000 # run this in another windows and wait for the packet ...
--pkg--
```
```bash
echo "u=net.createUDPSocket() u:send(5000, '192.168.1.5', '--pkg-') u:close()" |
curl -v -m 1 -d @- http://192.168.1.204/cmd
  * Hostname was NOT found in DNS cache
  *   Trying 192.168.1.204...
  * Connected to 192.168.1.204 (192.168.1.204) port 80 (#0)
  > POST /cmd HTTP/1.1
  > User-Agent: curl/7.35.0
  > Host: 192.168.1.204
  > Accept: */*
  > Content-Length: 71
  > Content-Type: application/x-www-form-urlencoded
  > 
  * upload completely sent off: 71 out of 71 bytes
  < HTTP/1.1 201 Completed
  * Server Niks NodeMCU on ESP8266 is not blacklisted
  < Server: Niks NodeMCU on ESP8266
  < Cache-control: no-store
  < Connection: close
  < 
  * Closing connection 0
```
