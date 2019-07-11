# HTTP Server Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-01-19 | [Vladimir Dronnikov](https://github.com/dvv) | [Vladimir Dronnikov](https://github.com/dvv) | [http.lua](../../lua_modules/http/httpserver.lua) |

This Lua module provides a simple callback implementation of a [HTTP 1.1](https://www.w3.org/Protocols/rfc2616/rfc2616.html) server.

!!!note
	copy this module to nodemcu by yourself 


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

- `send(data)`: Function to send data to client. `data` is data to send if the `data` is `nil` will close the connection

- `send_header(header)`: Function to send the response header. `header` must be a table. likes `{status = 200, headers={Connection = "close"}}`. status will be set 200 if the status omitted. the built-in code is `200, 304, 404, 500`

Full example can be found in [http-example.lua](../../lua_modules/http/http-example.lua)
