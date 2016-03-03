# HTTP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-01-15 | [Vowstar](https://github.com/vowstar) | [Vowstar](https://github.com/vowstar) | [http.c](../../../app/modules/http.c)|

Basic HTTP *client* module.

Provides an interface to do basic GET/POST/PUT/DELETE over HTTP(S), as well as customized requests. Due to the memory constraints on ESP8266, the supported page/body size is limited by available memory. Attempting to receive pages larger than this will fail. If larger page/body sizes are necessary, consider using [`net.createConnection()`](#netcreateconnection) and stream in the data.

Each request method takes a callback which is invoked when the response has been received from the server. The first argument is the status code, which is either a regular HTTP status code, or -1 to denote a DNS, connection or out-of-memory failure, or a timeout (currently at 10 seconds).

For each operation it is also possible to include custom headers. Note that following headers *can not* be overridden however:
  - Host
  - Connection
  - User-Agent

The `Host` header is taken from the URL itself, the `Connection` is always set to `close`, and the `User-Agent` is `ESP8266`.

Note that it is not possible to execute concurrent HTTP requests using this module. Starting a new request before the previous has completed will result in undefined behavior.

## http.delete()

Executes a HTTP DELETE request.

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
http.delete('https://connor.example.com/john',
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

Executes a HTTP GET request.

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
http.get("https://www.vowstar.com/nodemcu/", nil, function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```

## http.post()

Executes a HTTP POST request.

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
http.post('http://json.example.com/something',
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

Executes a HTTP PUT request.

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
http.put('http://db.example.com/items.php?key=deckard',
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

Execute a custom HTTP request for any HTTP method.

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
http.request("https://www.example.com", "HEAD", "", "", function(code, data)
  function(code, data)
    if (code < 0) then
      print("HTTP request failed")
    else
      print(code, data)
    end
  end)
```
