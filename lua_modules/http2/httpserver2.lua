-- A similar module to http/httpserver, more stable memory consumption

local collectgarbage, tonumber, tostring = collectgarbage, tonumber, tostring

local http

-- add some status code
local status_code = {}
status_code[200] = "200 OK"
status_code[304] = "304 Not Modified"
status_code[404] = "404 Not Found"
status_code[500] = "500 Internal Server Error"

------------------------------------------------------------------------------
-- request methods
------------------------------------------------------------------------------
local make_req = function(method, url)
  return {
    -- conn = conn,
    method = method,
    url = url,
  }
end

------------------------------------------------------------------------------
-- response methods
------------------------------------------------------------------------------
local make_res = function(csend, cfini)
  -- send("hello world\r\n") will send the data
  -- send() will close connection
  local send = function(data)
    if data then
      -- chunked transfer encoding
      csend(("%X\r\n"):format(#data))
      csend(data)
      csend("\r\n")
    else
      -- finalize chunked transfer encoding
      csend("0\r\n\r\n")
      -- close connection
      cfini()
    end
  end

  -- header = {status, headers = {connection = "close"}}
  local send_header = function(header)
    xpcall(function()
      assert(type(header) == "table", "header must be a table")
      -- status
      local check = status_code[header.status]
      if check == nil then check = 200 else check = status end
      csend(("HTTP/1.1 %s\r\n"):format(status_code[check]))
      csend("Transfer-Encoding: chunked\r\n")
      -- NB: quite a naive implementation
      if header.headers then
        assert(type(header.headers) == "table", "header[headers] must be a table")
        for k, v in pairs(header.headers) do
          csend(k)
          csend(": ")
          csend(v)
          csend("\r\n")
        end
      end
      -- NB: no headers allowed after response body started
      csend("\r\n")
    end, function(err)
      print(err)
    end, handler)
  end

  local res = { }
  res.send_header = send_header
  res.send = send
  return res
end

local http_handler = function(conn, handler)
  local buf = ""
  local method, url
  local req, res
  local csend = (require "fifosock").wrap(conn)

  local cfini = function()
    -- print("cfini")
    conn:on("receive", nil)
    csend(function()
      conn:on("sent", nil)
      conn:close()
      req = nil
      res = nil
      -- print("close")
    end)
  end
  -- header parser
  local cnt_len = 0
  local onheader = function(k, v)
    -- to help parse body
    -- parse content length to know body length
    if k == "content-length" then
      cnt_len = tonumber(v)
    end
    if k == "expect" and v == "100-continue" then
      csend("HTTP/1.1 100 Continue\r\n")
    end
    -- delegate to request object
    if req and req.onheader then
      req:onheader(k, v)
    end
  end
  -- body data handler
  local body_len = 0
  local ondata = function(sck, chunk)
    -- feed request data to request handler
    if not req or not req.ondata then return end
    body_len = body_len + #chunk
    if body_len >= cnt_len then
      -- last chunk
      req:ondata(chunk, true)
    else
      req:ondata(chunk, false)
    end
  end
  -- onreceive
  local onreceive = function(sck, chunk)
    -- merge chunks in buffer
    -- a big header ?
    if buf then
      buf = buf .. chunk
    else
      buf = chunk
    end
    -- consume buffer line by line
    while #buf > 0 do
      -- extract line
      local e = buf:find("\r\n", 1, true)
      if not e then break end
      local line = buf:sub(1, e - 1)
      buf = buf:sub(e + 2)
      -- method, url?
      if not method then
        local i
        -- NB: just version 1.1 assumed
        _, i, method, url = line:find("^([A-Z]+) (.-) HTTP/1.1$")
        if method then
          -- make request and response objects
          req = make_req(method, url)
          res = make_res(csend, cfini)
        end
        -- spawn request handler
        handler(req, res)
        -- header line?
      elseif #line > 0 then
        -- parse header
        local _, _, k, v = line:find("^([%w-]+):%s*(.+)")
        -- header seems ok?
        if k then
          k = k:lower()
          onheader(k, v)
        end
      else
        -- NB: we feed the rest of the buffer as starting chunk of body
        ondata(sck, buf)
        -- buffer no longer needed
        buf = nil
        -- NB: we explicitly reassign receive handler so that
        --   next received chunks go directly to body handler
        sck:on("receive", ondata)
        -- parser done
        collectgarbage("collect")
        break
      end
    end
  end
  -- ondisconnect
  local ondisconnect = function(sck)
    sck.on("sent", nil)
    req = nil
    res = nil
    collectgarbage("collect")
    -- print("ondisconnect")
  end
  -- event
  conn:on("receive", onreceive)
  conn:on("disconnection", ondisconnect)
end

------------------------------------------------------------------------------
-- HTTP server
------------------------------------------------------------------------------
local srv
local createServer = function(port, handler)
  -- NB: only one server at a time
  if srv then srv:close() end
  srv = net.createServer(net.TCP, 15)
  -- listen
  srv:listen(port, function(sck)
    http_handler(sck, handler)
  end)
  return srv
end

------------------------------------------------------------------------------
-- HTTP server methods
------------------------------------------------------------------------------
http = {
  createServer = createServer,
}

return http