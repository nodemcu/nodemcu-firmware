# LEDC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2021-11-07 | [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [httpd.c](../../components/modules/httpd.c)|

This module provides an interface to Espressif's [web server component](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html).


# HTTPD Overview

The httpd module implements support for both static file serving and dynamic
content generation. For static files, all files need to reside under a
common prefix (the "webroot") in the (virtual) filesystem. The module does
not care whether the underlying file system supports directories or not,
so files may be served from SPIFFS, FAT filesystems, or whatever else
may be mounted. If you wish to include the static website contents within
the firmware image itself, considering using the [EROMFS](eromfs.md) module.

Unlike the default behaviour of the Espressif web server, this module serves
static files based on file extensions primarily. Static routes are typically
defined as a file extension (e.g. \*.html) and the `Content-Type` such files
should be served as. A number of file extensions are included by default
and should cover the basic needs:

  - \*.html (text/html)
  - \*.css (text/css)
  - \*.js (text/javascript)
  - \*.json (application/json)
  - \*.gif (image/gif)
  - \*.jpg (image/jpeg)
  - \*.jpeg (image/jpeg)
  - \*.png (image/png)
  - \*.svg (image/svg+xml)
  - \*.ttf (font/ttf)

The native Espressif approach may also be used if you prefer, but is harder
to work with. Both schemes can coexist in most cases without issues. When
using the native approach, URI wildcard matching is supported.

Dynamic routes may be registered, which when accessed by a client will result
in a Lua function being invoked. This function may then generate whatever
content is applicable, for example obtaining a sensor value and returning it.

Note that if you are writing sensor data to files and serving those files
statically you will be susceptible to race conditions where the file contents
may not be available from the outside. This is due to the web server running
in its own FreeRTOS thread and serving files directly from that thread
concurrently with the Lua VM running as usual. It is therefore safer to
instead serve such content on a dynamic route, even if all that route does
is reads the file and serves that.

An example of such a setup:
```lua
function handler(req)
  local f = io.open('/path/to/mysensordata.csv', 'r')
  return {
    status = "200 OK",
    type = "text/plain",
    getbody = function()
      local data = f:read(512) -- pick a suiteable chunk size here
      if not data then f:close() end
      return data
    end,
  }
end

httpd.dynamic(httpd.GET, "/mysensordata", handler)
```

## httpd.start()
Starts the web server. The server has to be started before routes can be
configured.

#### Syntax
```lua
httpd.start({
  webroot = "<static file prefix>",
  max_handlers = 20,
  auto_index = httpd.INDEX_NONE || httpd.INDEX_ROOT || httpd.INDEX_ALL,
})
```

#### Parameters
A single configuration table is provided, with the following possible fields:

  - `webroot` (mandatory) This sets the prefix used when serving static files.
  For example, with `webroot` set to "web", a HTTP request for "/index.html"
  will result in the httpd module trying to serve the file "web/index.html"
  from the file system. Do NOT set this to the empty string, as that would
  provide remote access to your entire virtual file system, including special
  files such as virtual device files (e.g. "/dev/uart1") which would likely
  present a serious security issue.
  - `max_handlers` (optional) Configures the maximum number of route handlers
  the server will support. Default value is 20, which includes both the
  standard static file extension handlers and any user-provided handlers.
  Raising this will result in a bit of additional memory being used. Adjust
  if and when necessary.
  - `auto_index` Sets the indexer mode to be used. Most web servers
  automatically go looking for an "index.html" file when a directory is
  requested. For example, when pointing your web browser to a web site
  for the first time, e.g. http://www.example.com/ the actual request will
  come through for "/", which in turn commonly gets translated to "/index.html"
  on the server. This behaviour can be enabled in this module as well. There
  are three modes provided:
    - `httpd.INDEX_NONE` No automatic translation to "index.html" is provided.
    - `httpd.INDEX_ROOT` Only the root ("/") is translated to "/index.html".
    - `httpd.INDEX_ALL` Any path ending with a "/" has "index.html" appended.
    For example, a request for "subdir/" would become "subdir/index.html",
    which in turn might result in the file "web/subdir/index.html" being
    served (if the `webroot` was set to "web").
    The default value is `httpd.INDEX_ROOT`.

#### Returns
`nil`

#### Example
```lua
httpd.start({ webroot = "web", auto_index = httpd.INDEX_ALL })
```

## httpd.stop()

Stops the web server. All registered route handlers are removed.

#### Syntax
```lua
httpd.stop()
```

#### Parameters
None.

#### Returns
`nil`


## httpd.static()

Registers a static route handler.

#### Syntax
```
httpd.static(route, content_type)
```

#### Parameters
- `route` The route prefix. Typically in the form of \*.ext to serve all files
with the ".ext" extension statically. Refer to the Espressif [documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html)
if you wish to use the native Espressif style of static routes instead.
- `content_type` The value to send in the `Content-Type` header for this file
type.

#### Returns
An error code on failure, or `nil` on success. The error code is the value
returned from the `httpd_register_uri_handler()` function.

#### Example
```lua
httpd.start({ webroot = "web" })
httpd.static("*.csv", "text/csv") -- Serve CSV files under web/
```

## httpd.dynamic()

Registers a dynamic route handler.

#### Syntax
```lua
httpd.dynamic(method, route, handler)
```

#### Parameters
- `method` The HTTP method this route applies to. One of:
  - `httpd.GET`
  - `httpd.HEAD`
  - `httpd.PUT`
  - `httpd.POST`
  - `httpd.DELETE`
- `route` The route prefix. Be mindful of any trailing "/" as that may interact
with the `auto_index` functionality.
- `handler` The route handler function - `handler(req)`. The provided request
object `req` has the following fields/functions:
  - `method` The request method. Same as the `method` parameter above. If the
  same function is registered for several methods, this field can be used to
  determine the method the request used.
  - `uri` The requested URI. Includes both path and query string (if
  applicable).
  - `query` The query string on its own. Not decoded.
  - `headers` A table-like object in which request headers may be looked up.
  Note that due to the Espressif API not providing a way to iterate over all
  headers this table will appear empty if fed to `pairs()`.
  - `getbody()` A function which may be called to read in the request body
  incrementally. The size of each chunk is set via the Kconfig option
  "Receive body chunk size". When this function returns `nil` the end of
  the body has been reached. May raise an error if reading the body fails
  for some reason (e.g. timeout, network error).

Note that the provided `req` object is _only valid_ within the scope of this
single invocation of the handler. Attempts to store away the request and use
it later _will_ fail.

#### Returns
A table with the response data to send to the requesting client:
```lua
{
  status = "200 OK",
  type = "text/plain",
  headers = {
    ['X-Extra'] = "My custom header value"
  },
  body = "Hello, Lua!",
  getbody = dynamic_content_generator_func,
}
```
Supported fields:
- `status` The status code and string to send. If not included "200 OK" is used.
Other common strings would be "404 Not Found", "400 Bad Request" and everybody's
favourite "500 Internal Server Error".
- `type` The value for the `Content-Type` header. The Espressif web server
component handles this header specially, which is why it's provided here and
not within the `headers` table.
- `body` The full content body to send.
- `getbody` A function to source the body content from, similar to the way
the request body is read in. This function will be called repeatedly and the
returned string from each invocation will be sent as a chunk to the client.
Once this function returns `nil` the body is deemed to be complete and no
further calls to the function will be made. It is guaranteed that the
function will be called until it returns `nil` even if the sending of the
content encounters an error. This ensures that any resource cleanup
necessary will still take place in such circumstances (e.g. file closing).

Only one of `body` and `getbody` should be specified.

#### Example
```lua
httpd.start({ webroot = "web" })

function put_foo(req)
  local body_len = tonumber(req.headers['content-length']) or 0
  if body_len < 4096
  then
    local f = io.open("/upload/foo.txt", "w")
    local body = req.getbody()
    while body
    do
      f:write(body)
      body = req.getbody()
    end
    f:close()
    return { status = "201 Created" }
  else
    return { status = "400 Bad Request" }
  end
end

httpd.dynamic(httpd.PUT, "/foo", put_foo)
```

## httpd.unregister()

Unregisters a previously registered handler. The default handlers may be
unregistered.

#### Syntax
```lua
httpd.unregister(method, route)
```

#### Parameters
- `method` The method the route was registered for. One of:
  - `httpd.GET`
  - `httpd.HEAD`
  - `httpd.PUT`
  - `httpd.POST`
  - `httpd.DELETE`
- `route` The route prefix.

#### Returns
`1` on success, `nil` on failure (including if the route was not registered).

#### Example
Unregistering one of the default static handlers:
```lua
httpd.start({ webroot = "web" })
httpd.unregister(httpd.GET, "*.jpeg")
```
