# HTTP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-10-27 | [Tom Sutcliffe](https://github.com/tomsci) | [Tom Sutcliffe](https://github.com/tomsci) | [http.c](../../../components/modules/http.c)|

HTTP *client* module that provides an interface to do GET/POST/PUT/DELETE over HTTP and HTTPS, as well as customized requests. It can support large requests with an API similar to that of the net module. 

!!! attention

    It is **not** possible to execute concurrent HTTP requests using this module. 

Each request method takes a callback which is invoked when the response has been received from the server. The first argument is the status code, which is either a regular HTTP status code, or -1 to denote a DNS, connection or out-of-memory failure, or a timeout (currently at 10 seconds).

For each operation it is possible to provide custom HTTP headers or override standard headers. By default the `Host` header is deduced from the URL and `User-Agent` is `ESP32 HTTP Client/1.0`. Requests are always sent as `HTTP/1.1`. Keep-alive is supported (unless using the one-shot APIs) by default, disable by adding a `Connection: close` header.

HTTP redirects (HTTP status 300-308) are followed automatically up to a limit of 10 to avoid the dreaded redirect loops.

Whenever headers are passed into a callback, the header names are always lower cased. If there are multiple headers of the same name, then only the last one is returned.

## http.createConnection()

Creates a connection object which can be configured and then executed. Note this function does not actually open the connection to the remote server, that doesn't happen until `connection:request()` is called.

#### Syntax
`http.createConnection([url[, method[, headers]]])`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix. Optional, in which case the url (or host and port, etc) must be set before `request()` is called.
- `method` The HTTP method to use, one of `http.GET`, `http.POST`, `http.DELETE` or `http.HEAD`. Optional, the default is `http.GET`.
- `headers` Optional additional headers to append, as a table; may be `nil`.

#### Returns
The connection object

#### Example
```lua
headers = {
  Connection = "close",
  ["If-Modified-Since"] = "Sat, 27 Oct 2018 00:00:00 GMT",
}
connection = http.createConnection("http://httpbin.org/", http.GET, headers)
connection:on("finish", function(status)
  print("Completed status code =", status)
end)
connection:request()
```

# http connection objects

## connection:on()
Set a callback to be called when a certain event occurs.

#### Syntax
`connection:on(event[, callback])`

#### Parameters
- `event` One of
    - `"connect"` Called when the connection is first established. Callback receives no arguments.
    - `"headers"` Called once the HTTP headers from the remote end have been received. Callback is called as `callback(status_code, headers_table)`.
    - `"data"` Can be called multiple times, each time more (non-headers) data is received. Callback is called as `callback(status_code, data)`.
    - `"finish"` Called once all data has been received. Callback is called as `callback(status_code)`.
- `callback` a function to be called when the given event occurs. Can be `nil` to remove a previously configured callback.

If an error occurs, the `status_code` in the callback will be `-1`. The only callback guaranteed to be called in an error situation is `finish`.

Note, you must not attempt to reuse `connection` from within its `finish` callback (or from any of the callbacks). The connection object may only be reused once `request()` has returned. You may call `close()` from within a callback to indicate that the connection should be aborted.

#### Returns
`nil`

#### Example
See `http.createConnection()`.

## connection:request()
Opens the connection to the server and issues the request. All callbacks happen in the context of this function, which doesn't return until the request is complete.

#### Syntax
`connection:request()`

#### Parameters
None

#### Returns
`true` if the connection is still open, `false` otherwise. If this returns `true` and you don't wish to use the socket again, you should call `connection:close()` to ensure the socket is not kept open unnecessarily. When a connection is garbage collected any remaining sockets will be closed. 

## connection:setmethod()
Sets the connection method. Useful if making multiple requests of different types on a single connection.

#### Syntax
`connection:setmethod(method)`

#### Parameters
- `method` one of `http.GET`, `http.POST`, `http.HEAD`, `http.DELETE`.

#### Returns
`nil`

#### Example
```lua
connection:setmethod(http.POST)
```

## connection:seturl()
Sets the connection URL. Useful if making multiple requests on a single connection.

#### Syntax
`connection:seturl(url)`

#### Parameters
- `url` Required. the URL to use for the next `request()` call

#### Returns
`nil`

#### Example
```lua
connection = http.createConnection()
connection:seturl("http://httpbin.org/ip")
connection:request()
```

## connection:setheader()
Sets an individual header in the request. Header names are case-insensitive, but it is recommended to match the case of any headers automatically added by the underlying library (for example: `Connection`, `Content-Type`, `User-Agent` etc).

#### Syntax
`connection:setheader(name[, value])`

#### Parameters
- `name` name of the header to set.
- `value` what to set it to. Must be a string, or `nil` to unset it.

## connection:setpostdata()
Sets the POST data to be used for this request. Also sets the method to `http.POST` if it isn't already. If a `Content-Type` header has not already been set, also sets that to `application/x-www-form-urlencoded`.

#### Syntax
`connection:setpostdata([data])`

#### Parameters
`data` - The data to POST. Unless a custom `Content-Type` header has been set, this data should be in `application/x-www-form-urlencoded` format. Can be `nil` to unset what to post and the `Content-Type` header.

## connection:close()
Closes the connection if it is still open. Note this does not reset any configured URL or callbacks on the connection object, it just closes the underlying TCP/IP connection. The connection object may be used for subsequent requests after being closed.

# One shot HTTP APIs
These APIs are wrappers around the connection object based APIs above. They allow simpler code to be written for one-off requests where complex configuration and/or connection object tracking is not required. Note however that they buffer the incoming data and therefore are limited by free memory in how large a request they can handle. The EGC can also artificially limit this - try setting `node.egc.setmode(node.egc.ON_ALLOC_FAILURE)` for more optimal memory management behavior. If however there is physically not enough RAM to buffer the entire request, then the connection object based APIs must be used instead so each individual `data` callback can be processed separately.

All one-shot APIs add the header `Connection: close` regardless of what the `headers` parameter contains.

## http.get()
Make an HTTP GET request.

#### Syntax
`http.get(url, [headers,] callback)`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `headers` Table of additional headers to append. May be `nil` or ommitted.
- `callback` The callback function to be invoked when the response has been received or an error occurred; it is invoked with the arguments `status_code`, `body` and `headers`. In case of an error `status_code` is set to -1.

#### Returns
`nil`

#### Example
```lua
http.get("http://httpbin.org/ip", nil, function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```

## http.post()

Executes a single HTTP POST request and closes the connection.

#### Syntax
`http.post(url, headers, body, callback)`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `headers` Table of additional headers to append. May be `nil`.
- `body` The body to post. Must already be encoded in the appropriate format, but may be empty. See `connection:setpostdata()` for more information.
- `callback` The callback function to be invoked when the response has been received or an error occurred; it is invoked with the arguments `status_code`, `body` and `headers`. In case of an error `status_code` is set to -1.

#### Returns
`nil`

#### Example
```lua
headers = {
  ["Content-Type"] = "application/json",
}
body = '{"hello":"world"}'
http.post("http://httpbin.org/post", headers, body,
  function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```
