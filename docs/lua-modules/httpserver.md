# HTTP Server Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-01-19 | [Vladimir Dronnikov](https://github.com/dvv) | [Vladimir Dronnikov](https://github.com/dvv) | [http.lua](../../lua_modules/http/httpserver.lua) |

This Lua module provides a simple callback implementation of a [HTTP 1.1](https://www.w3.org/Protocols/rfc2616/rfc2616.html) server.

!!!note
	copy this file to the nodemcu by yourself

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

- `ondata`: value to setup handler function HTTP data. Handler function has 3 parameters:
	- `self`: `req` object
	- `chunk`: Request data
	- `last`: set true while the last chunk

The second object holds functions:  

- `send_header(header)`: header must be a table like `{status = 200, headers = {connection = "close"}}`. If the status omit,  header.status will be set 200. Built-in 4 status code: 

	- `200 OK`
	- `304 Not Modified`
	- `404 Not Found`
	- `500 Internal Server Error`

- `send(data)`: Function to send the data to client. if `data` is nil will close connection. **so, `send()` is always on the last.**


Full example can be found in [http-example.lua](../../lua_modules/http/http-example.lua)
