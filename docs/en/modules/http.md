# HTTP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-01-15 | [esphttpclient](https://github.com/Caerbannog/esphttpclient) / [Vowstar](https://github.com/vowstar) | [Vowstar](https://github.com/vowstar) | [http.c](../../../app/modules/http.c)|

Basic HTTP *client* module that provides an interface to do GET/POST/PUT/DELETE over HTTP(S), as well as customized requests. Due to the memory constraints on ESP8266, the supported page/body size is limited by available memory. Attempting to receive pages larger than this will fail. If larger page/body sizes are necessary, consider using [`net.createConnection()`](net.md#netcreateconnection) and stream in the data.

!!! attention

    It is **not** possible to execute concurrent HTTP requests using this module. Starting a new request before the previous has completed will result in undefined behavior. Use [`node.task.post()`](https://nodemcu.readthedocs.io/en/master/en/modules/node/#nodetaskpost) in the callbacks of your calls to start subsequent calls if you want to chain them (see [#1258](https://github.com/nodemcu/nodemcu-firmware/issues/1258)).

Each request method takes a callback which is invoked when the response has been received from the server. The first argument is the status code, which is either a regular HTTP status code, or -1 to denote a DNS, connection or out-of-memory failure, or a timeout (currently at 10 seconds).

For each operation it is also possible to include custom headers. Note that following headers *can not* be overridden however:

- Host
- Connection
- User-Agent

The `Host` header is taken from the URL itself, the `Connection` is always set to `close`, and the `User-Agent` is `ESP8266`.

**SSL/TLS support**

Take note of constraints documented in the [net module](net.md). 

## http.delete()

Executes a HTTP DELETE request. Note that concurrent requests are not supported.

#### Syntax
`http.delete(url, headers, body, callback)`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `headers` Optional additional headers to append, *including \r\n*; may be `nil`
- `body` The body to post; must already be encoded in the appropriate format, but may be empty
- `callback` The callback function to be invoked when the response has been received; it is invoked with the arguments `status_code` and `body`

#### Returns
`nil`

#### Example
```lua
http.delete('http://httpbin.org/delete',
  "",
  "",
  function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```

## http.get()

Executes a HTTP GET request. Note that concurrent requests are not supported.

#### Syntax
`http.get(url, headers, callback)`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `headers` Optional additional headers to append, *including \r\n*; may be `nil`
- `callback` The callback function to be invoked when the response has been received; it is invoked with the arguments `status_code` and `body`

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

Executes a HTTP POST request. Note that concurrent requests are not supported.

#### Syntax
`http.post(url, headers, body, callback)`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `headers` Optional additional headers to append, *including \r\n*; may be `nil`
- `body` The body to post; must already be encoded in the appropriate format, but may be empty
- `callback` The callback function to be invoked when the response has been received; it is invoked with the arguments `status_code` and `body`

#### Returns
`nil`

#### Example
```lua
http.post('http://httpbin.org/post',
  'Content-Type: application/json\r\n',
  '{"hello":"world"}',
  function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```

## http.put()

Executes a HTTP PUT request. Note that concurrent requests are not supported.

#### Syntax
`http.put(url, headers, body, callback)`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `headers` Optional additional headers to append, *including \r\n*; may be `nil`
- `body` The body to post; must already be encoded in the appropriate format, but may be empty
- `callback` The callback function to be invoked when the response has been received; it is invoked with the arguments `status_code` and `body`

#### Returns
`nil`

#### Example
```lua
http.put('http://httpbin.org/put',
  'Content-Type: text/plain\r\n',
  'Hello!\nStay a while, and listen...\n',
  function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```

## http.request()

Execute a custom HTTP request for any HTTP method. Note that concurrent requests are not supported.

#### Syntax
`http.request(url, method, headers, body, callback)`

#### Parameters
- `url` The URL to fetch, including the `http://` or `https://` prefix
- `method` The HTTP method to use, e.g. "GET", "HEAD", "OPTIONS" etc
- `headers` Optional additional headers to append, *including \r\n*; may be `nil`
- `body` The body to post; must already be encoded in the appropriate format, but may be empty
- `callback` The callback function to be invoked when the response has been received; it is invoked with the arguments `status_code` and `body`

#### Returns
`nil`

#### Example
```lua
http.request("http://httpbin.org", "HEAD", "", "", 
  function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```
