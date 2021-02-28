# HTTP Server Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-01-19 | [Vladimir Dronnikov](https://github.com/dvv) | [Vladimir Dronnikov](https://github.com/dvv) | [http.lua](../../lua_modules/http/httpserver.lua) |

This Lua module provides a simple callback implementation of a [HTTP 1.1](https://www.w3.org/Protocols/rfc2616/rfc2616.html) server.

### Require
```lua
httpserver = require("httpserver")
```

### Release
```lua
httpserver = nil
package.loaded["httpserver"] = nil
```

## httpserver.createServer()
Function to start HTTP server.

#### Syntax
`httpserver.createServer(port, handler(req, res))`

#### Parameters
- `port`: Port number for HTTP server. Most HTTP servers listen at port 80.
- `handler`: callback function for when HTTP request was made.

#### Returns
`net.server` sub module.

#### Notes
Callback function has 2 arguments: `req` (request) and `res` (response). The first object holds values:

- `conn`: `net.socket` sub module.  **DO NOT** call `:on` or `:send` on this
  object.
- `method`: Request method that was used (e.g.`POST` or `GET`)
- `url`: Requested URL
- `onheader`: assign a function to this value which will be called as soon as HTTP headers like `content-type` are available.
              This handler function has 3 parameters:
	- `self`: `req` object
	- `name`: Header name. Will allways be lowercase.
	- `value`: Header value

- `ondata`: assign a function to this value which will be called as soon as body data is available. 
            This handler function has 2 parameters:
	- `self`: `req` object
	- `chunk`: Request data. If all data is received there will be one last call with data = nil

The second object holds functions:

- `send(self, data, [response_code])`: Function to send data to client. 
	- `self`: `res` object
	- `data`: data to send (may be nil)
	- `response_code`: the HTTP response code like `200`(default) or `404` (for example) *NOTE* if there are several calls with response_code given only the first one will be used. Any further codes given will be ignored.
  
- `send_header(self, header_name, header_data)`: Function to send HTTP headers to client. This function will not be available after data has been sent. (It will be nil.)
	- `self`: `res` object
	- `header_name`: the HTTP header name
	- `header_data`: the HTTP header data

- `finish([data[, response_code]])`: Function to finalize connection, optionally sending data and return code.

	- `data`: optional data to send on connection finalizing
	- `response_code`: the HTTP response code like `200`(default) or `404` (for example) *NOTE* if there are several calls with response_code given only the first one will be used. Any further codes given will be ignored.

Full example can be found in [http-example.lua](../../lua_modules/http/http-example.lua)
