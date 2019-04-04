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
- `onheader`: value to setup handler function for HTTP headers like `content-type`. Handler function has 3 parameters:

	- `self`: `req` object
	- `name`: Header name
	- `value`: Header value

- `ondata`: value to setup handler function HTTP data. Handler function has 2 parameters:
	- `self`: `req` object
	- `chunk`: Request data

The second object holds functions:

- `send(self, data, [response_code])`: Function to send data to client. `self` is `req` object, `data` is data to send and `response_code` is HTTP response code like `200` or `404` (for example)
- `send_header(self, header_name, header_data)`: Function to send HTTP headers to client. `self` is `req` object, `header_name` is HTTP header name and `header_data` is HTTP header data for client.
- `finish([data])`: Function to finalize connection, optionally sending data. `data` is optional data to send on connection finalizing.

Full example can be found in [http-example.lua](../../lua_modules/http/http-example.lua)
