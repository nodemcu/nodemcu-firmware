# HTTP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-10-27 | [Tom Sutcliffe](https://github.com/tomsci) | [Tom Sutcliffe](https://github.com/tomsci) | [http.c](../../components/modules/http.c)|

HTTP *client* module that provides an interface to do GET/POST/PUT/DELETE over HTTP and HTTPS, as well as customized requests. It can support large requests with an API similar to that of the `net` module. Multiple concurrent HTTP requests are supported. Both synchronous and asynchronous modes are supported.

For each operation it is possible to provide custom HTTP headers or override standard headers. By default the `Host` header is deduced from the URL and `User-Agent` is `NodeMCU (ESP32)`. Requests are always sent as `HTTP/1.1`. Keep-alive is supported (unless using the one-shot APIs) by default, disable by adding a `Connection: close` header or by explicitly closing the connection once complete.

HTTP redirects (HTTP status 300-308) are followed automatically up to a limit of 10 to avoid redirect loops. This behavior may be customized by setting the `max_redirects` option.

Whenever headers are returned or passed into a callback, the header names are always lower cased. If there are multiple headers of the same name, then only the last one is returned.

## http.createConnection()

Creates a connection object which can be configured and then executed. Note this function does not actually open the connection to the remote server, that doesn't happen until [`connection:request()`](#connectionrequest) is called.

#### Syntax
`http.createConnection(url, [method,] [options])`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix. Required.
- `method` The HTTP method to use, one of `http.GET`, `http.POST`, `http.PUT`, `http.DELETE` or `http.HEAD`. Optional and may be omitted, the default is `http.GET`.
- `options` An optional table containing any or all of:
    - `async` If true, the request is processed asynchronously, meaning [`request()`](#connectionrequest) returns immediately rather than blocking until the connection is complete and all callbacks have been made. Some other connection APIs behave differently in asynchronous mode, see their documentation for details. If not specified, the default is `false`, meaning requests are processed synchronously.
    - `bufsz` The size in bytes of the temporary buffer used for reading data. If not specified, the default is `512`.
    - `cert` A [PEM-encoded](https://en.wikipedia.org/wiki/Privacy-Enhanced_Mail) certificate (or certificates). If specified, the server's TLS certificate must chain back to one of these root or intermediate certificates otherwise the request will fail. This option is ignored for HTTP requests (unless they redirect to an HTTPS URL).
    - `headers` Table of headers to add to the request.
    - `max_redirects` Maximum number of `30x` redirects to follow before giving up. If not specified, the default is `10`. Specify `0` to disable following redirects entirely. 
    - `timeout` Network timeout, in milliseconds. If not specified, the default is `10000` (10 seconds).

#### Returns
`connection` The connection object.

#### Example
```lua
headers = {
  Connection = "close",
  ["If-Modified-Since"] = "Sat, 27 Oct 2018 00:00:00 GMT",
}
connection = http.createConnection("http://httpbin.org/", http.GET, { headers=headers } )
connection:on("complete", function(status)
  print("Request completed with status code =", status)
end)
connection:request()
```

# HTTP connection objects

## connection:on()
Set a callback to be called when a certain event occurs.

#### Syntax
`connection:on(event[, callback])`

#### Parameters
- `event` One of
    - `"connect"` Called when the connection is first established. Callback receives no arguments.
    - `"headers"` Called once the HTTP headers from the remote end have been received. Callback is called as `callback(status_code, headers_table)`.
    - `"data"` Can be called multiple times, each time more (non-headers) data is received. Callback is called as `callback(status_code, data)`.
    - `"complete"` Called once all data has been received. Callback is called as `callback(status_code, connected)` where `connected` is `true` if the connection is still open.
- `callback` a function to be called when the given event occurs. Can be `nil` to remove a previously configured callback.

If an error occurs, the `status_code` in the callback will be a negative number. The only callback guaranteed to be called in an error situation is `complete`.

If the connection is asynchronous, the `data` callback may optionally return the constant `http.DELAYACK` to indicate that no further callbacks should be made until [`connection:ack()`](#connectionack) is called. This is useful to slow down the arrival of data in cases where the system is not ready to receive any more. If desired, non-delayed acks may be indicated by returning `http.ACKNOW` but this is not required since it's the default behaviour anyway.

Note, you must not attempt to reuse `connection` until the  `complete` callback has been called, or (in synchronous mode) until `request()` returns. You may reuse the connection from within the `complete` callback (in either mode). You may call `connection:close()` from within any callback to indicate that the connection should be aborted; in any callback other than `complete`, this will result in a subsequent `complete` callback.

#### Returns
`nil`

#### Example
See [`http.createConnection()`](#httpcreateconnection).

## connection:request()
Opens the connection to the server and issues the request. If `async` was set to `true` in the connection options, then this function returns immediately. Otherwise, the function doesn't return until all callbacks have been made and the request is complete. If the server supports Keep-Alive and `Connection = "close"` wasn't specified in the headers, the connection will remain open after completion, ready for another call to `request()` (possibly after setting a new URL with `connection:seturl()`).

#### Syntax
`connection:request()`

#### Parameters
None

#### Returns
In asynchronous mode, always returns `nil`.

In synchronous mode, it returns 2 results, `status_code, connected` where `connected` is `true` if the connection is still open. In that case, if you don't wish to use the socket again, you should call `connection:close()` to ensure the socket is not kept open unnecessarily. When a connection is garbage collected any remaining sockets will be closed.

## connection:setmethod()
Sets the connection method. Useful if making multiple requests of different types on a single connection. Errors if called while a request is in progress.

#### Syntax
`connection:setmethod(method)`

#### Parameters
- `method` one of `http.GET`, `http.POST`, `http.PUT`, `http.HEAD`, `http.DELETE`.

#### Returns
`nil`

#### Example
```lua
connection:setmethod(http.POST)
```

## connection:seturl()
Sets the connection URL. Useful if making multiple requests on a single connection. Errors if called while a request is in progress.

#### Syntax
`connection:seturl(url)`

#### Parameters
- `url` Required. The URL to use for the next `request()` call.

#### Returns
`nil`

#### Example
```lua
connection = http.createConnection("http://httpbin.org/")
connection:request()
-- Make another request
connection:seturl("http://httpbin.org/ip")
connection:request()
```

## connection:setheader()
Sets an individual header in the request. Header names are case-insensitive, but it is recommended to match the case of any headers automatically added by the underlying library (for example: `Connection`, `Content-Type`, `User-Agent` etc). Errors if called while a request is in progress.

#### Syntax
`connection:setheader(name[, value])`

#### Parameters
- `name` name of the header to set.
- `value` what to set it to. Must be a string, or `nil` to unset it.

## connection:setbody()
Sets the body data to be used for this request (for POST, PUT, etc). If a `Content-Type` header has not already been set, also sets that to `application/x-www-form-urlencoded`. Errors if called while a request is in progress.

#### Syntax
`connection:setbody([data])`

#### Parameters
`data` The data to POST/PUT/etc.. Unless a custom `Content-Type` header has been set, this data should be in `application/x-www-form-urlencoded` format. Can be `nil` to unset what to post and the `Content-Type` header.

#### Returns
`nil`

## connection:ack()
Completes a callback that was previously delayed by returning `http.DELAYACK`. This unblocks the request and allows subsequent callbacks to occur. Errors if the most recent callback wasn't an asynchronous one that was delayed.

#### Syntax
`connection:ack()`

#### Parameters
None

#### Returns
`nil`

## connection:close()
Closes the connection if it is still open. Note this does not reset any configured URL or callbacks on the connection object, it just closes the underlying TCP/IP socket. If called from within a callback, this aborts the connection, but the connection object must not be reused until the `complete` callback occurs (waiting until after `request()` returns is also allowed, if the request was synchronous). In the case of a connection in asynchronous mode, will error if called from outside of a callback while a request is ongoing, _unless_ the last callback returned `http.DELAYACK` _and_ `connection:ack()` has not yet been called. In such a situation, `connection:ack()` must still be called, after the call to `connection:close()`. An asynchronous connection that is still open but not ongoing (ie the `complete` callback has taken place) may be closed without restriction. 

#### Syntax
`connection:close()`

#### Parameters
None

#### Returns
`nil`

# One shot HTTP APIs
These APIs are wrappers around the connection object based APIs above. They allow simpler code to be written for one-off requests where complex connection object management is not required. Note however that they buffer the incoming data and therefore are limited by free memory in how large a request they can handle. The EGC can also artificially limit this - try setting `node.egc.setmode(node.egc.ON_ALLOC_FAILURE)` for more optimal memory management behavior. If however there is physically not enough RAM to buffer the entire request, then the connection object based APIs must be used instead so each individual `data` callback can be processed separately.

All one-shot APIs add the header `Connection: close` regardless of what the `options.headers` parameter contains, and can be executed either synchronously or asynchronously, depending on whether a `callback` is specified, regardless of any `options.async` setting.

* Synchronous mode: The call to `get()`/`post()` does not return until the request is complete, and the results of the request are returned from the call.
* Asynchronous mode: The call returns immediately (with no results), the results of the request are given as arguments to the `callback` function.

If more advanced control over the request is required, the connection object based APIs [`http.createConnection()`](#httpcreateconnection) and [`connection:request()`](#connectionrequest) should be used instead.

## http.get()
Make an HTTP GET request. If a `callback` is specifed then the function operates in asynchronous mode, otherwise it is synchronous.

#### Syntax
`http.get(url, [options,] [callback])`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `options` Same options as [`http.createConnection()`](#httpcreateconnection), except that `async` is set for you based on whether a `callback` is specified or not. May be `nil` or omitted.
- `callback` Should be `nil` or omitted to specify synchronous behaviour, otherwise a callback function to be invoked when the response has been received or an error occurred, which is called with the arguments `status_code`, `body` and `headers`. In case of an error `status_code` will be a negative number.

#### Returns
In synchronous mode, returns 3 results `status_code, body, headers` once the request has completed. In asynchronous mode, returns `nil` immediately. 

#### Example
```lua
-- Asynchronous mode
http.get("http://httpbin.org/ip", function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
  
-- Synchronous mode
code, data = http.get("http://httpbin.org/ip")
if (code < 0) then
    print("HTTP request failed")
else
    print(code, data)
end
```

## http.post()

Executes a single HTTP POST request and closes the connection. If a `callback` is specifed then the function operates in asynchronous mode, otherwise it is synchronous.

#### Syntax
`http.post(url, options, body[, callback])`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `options` Same options as [`http.createConnection()`](#httpcreateconnection), except that `async` is set for you based on whether a `callback` is specified or not. May be `nil`.
- `body` The body to post. Required and must already be encoded in the appropriate format, but may be empty. See [`connection:setbody()`](#connectionsetbody) for more information.
- `callback` Should be `nil` or omitted to specify synchronous mode, otherwise a callback function to be invoked when the response has been received or an error occurred, which is called with the arguments `status_code`, `body` and `headers`. In case of an error `status_code` will be a negative number.

#### Returns
In synchronous mode, returns 3 results `status_code, body, headers` once the request has completed. In asynchronous mode, returns `nil` immediately. 

#### Example
```lua
headers = {
  ["Content-Type"] = "application/json",
}
body = '{"hello":"world"}'
http.post("http://httpbin.org/post", { headers = headers }, body,
  function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```

## http.put()

Executes a single HTTP PUT request and closes the connection. If a `callback` is specifed then the function operates in asynchronous mode, otherwise it is synchronous.

#### Syntax
`http.put(url, options, body[, callback])`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `options` Same options as [`http.createConnection()`](#httpcreateconnection), except that `async` is set for you based on whether a `callback` is specified or not. May be `nil`.
- `body` The body to post. Required and must already be encoded in the appropriate format, but may be empty. See [`connection:setbody()`](#connectionsetbody) for more information.
- `callback` Should be `nil` or omitted to specify synchronous mode, otherwise a callback function to be invoked when the response has been received or an error occurred, which is called with the arguments `status_code`, `body` and `headers`. In case of an error `status_code` will be a negative number.

#### Returns
In synchronous mode, returns 3 results `status_code, body, headers` once the request has completed. In asynchronous mode, returns `nil` immediately. 
