# HTTP Server Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-01-19 | [hanfengcan](https://github.com/hanfengcan) | [hanfengcan](https://github.com/hanfengcan) | [httpserver2.lua](../../lua_modules/http2/httpserver2.lua) |

A similar module to [http/httpserver](../../lua_modules/http/httpserver.lua), more stable memory consumption


!!! note
	Copy this module and upload to your nodemcu by yourself.  

### Require
```lua
httpserver = require("httpserver2")
```

### Release
```lua
httpserver = nil
package.loaded["httpserver"] = nil
```

### httpserver.createServer()
Function to start HTTP server.

#### Syntax
`httpserver.createServer(port, handler(req, res))`

#### Parameters
- `port`: Port number for HTTP server. Most HTTP servers listen at port 80.
- `handler`: callback function for when HTTP request was made. It has 2 arguments: `req` (request) and `res` (response).  
	The first object holds values:

	- `method`: Request method that was used (e.g.`POST` or `GET`).
	- `url`: Requested URL.
	- `onheader`: value to setup handler function for HTTP headers like `content-type`. Handler function has 3 parameters:

		- `self`: `req` object.
		- `name`: Header name.
		- `value`: Header value.

	- `ondata`: value to setup handler function HTTP data. Handler function has 3 parameters:
		- `self`: `req` object.
		- `chunk`: Request data.
		- `last`: `true` while received all request data, do your work after last is true.

	The second object holds functions:

	- `send_header(header)`: header must be a table like `{status = 200, headers = {connection = "close"}}`. If the status omit,  header.status will be set 200. Built-in 4 status code: 

		+ `200 OK`
		+ `304 Not Modified`
		+ `404 Not Found`
		+ `500 Internal Server Error`


	- `send(data)`: Function to send the data to client. if `data` is nil will close connection. **so, `send()` is always on the last.**

#### Returns
`net.server` sub module.

### example 

Here is a simple demo to test this module. recommend use **postman**. `curl` is good too.

```curl
<!-- post the message   -->

curl 192.168.3.40 -d "hello world!"

<!-- get request -->

curl 192.168.3.40/?hello=world
```

```lua
require("httpserver2").createServer(80, function(req, res)
  -- analyse method and url
  print("+R", req.method, req.url, node.heap())
  -- setup handler of headers, if any
  req.onheader = function(self, name, value)
    print("+H", name, value)
    -- E.g. look for "content-type" header,
    --   setup body parser to particular format
    -- if name == "content-type" then
    --   if value == "application/json" then
    --     req.ondata = function(self, chunk) ... end
    --   elseif value == "application/x-www-form-urlencoded" then
    --     req.ondata = function(self, chunk) ... end
    --   end
    -- end
  end
  -- setup handler of body, if any
  req.ondata = function(self, chunk, last)
    print("+B", chunk and #chunk, node.heap())
    if last then
      -- reply
      res.send_header({headers = {connection = "close"}})
      res.send(chunk, "\r\n")
      res.send() -- don't forget this line
    end
  end
end)
```
