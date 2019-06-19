------------------------------------------------------------------------------
-- HTTP server module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
------------------------------------------------------------------------------
local collectgarbage, tonumber, tostring = collectgarbage, tonumber, tostring

local http

-- add some status code
local status_code = {}
status_code[200] = "200 OK"
status_code[304] = "304 Not Modified"
status_code[404] = "404 Not Found"
status_code[500] = "500 Internal Server Error"

do
  ------------------------------------------------------------------------------
  -- request methods
  ------------------------------------------------------------------------------
  local make_req = function(conn, method, url)
    return {
      conn = conn,
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
    local send_header = function(header)
      if type(header) ~= "table" then
        print("httpserver.lua: header need a table! eg: {status = 200, headers = {Connection = \"close\"}}")
        return
      end
      if type(header.headers) ~= "table" then
        print("httpserver.lua: header.headers need a table!")
        return
      end
      -- status
      local check = status_code[header.status]
      if check == nil then check = 200 else check = header.status end
      csend(("HTTP/1.1 %s\r\n"):format(status_code[check]))
      csend("Transfer-Encoding: chunked\r\n")
      -- header
      for k, v in pairs(header.headers) do
        csend(k)
        csend(": ")
        csend(v)
        csend("\r\n")
      end
      csend("\r\n")
    end

    local res = { }
    res.send_header = send_header
    res.send = send
    -- res.finish = finish -- no need, use send()
    return res
  end

  ------------------------------------------------------------------------------
  -- HTTP parser
  ------------------------------------------------------------------------------
  local http_handler = function(handler)
    return function(conn)
      local req, res
      local buf = ""
      local method, url
      local csend = (require "fifosock").wrap(conn)
      local cfini = function()
        conn:on("receive", nil)
        conn:on("disconnection", nil)
        csend(function() conn:on("sent", nil) conn:close() res = nil req = nil end)
      end
      local ondisconnect = function(conn)
        conn:on("sent", nil)
        res = nil
        req = nil
        collectgarbage("collect")
      end
      -- header parser
      local cnt_len = 0
      local onheader = function(conn, k, v)
        -- TODO: look for Content-Type: header
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
      local ondata = function(conn, chunk)
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
      local onreceive = function(conn, chunk)
        -- merge chunks in buffer
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
              req = make_req(conn, method, url)
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
              onheader(conn, k, v)
            end
          -- headers end
          else
            -- NB: we feed the rest of the buffer as starting chunk of body
            ondata(conn, buf)
            -- buffer no longer needed
            buf = nil
            -- NB: we explicitly reassign receive handler so that
            --   next received chunks go directly to body handler
            conn:on("receive", ondata)
            -- parser done
            break
          end
        end
      end
      conn:on("receive", onreceive)
      conn:on("disconnection", ondisconnect)
    end
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
    srv:listen(port, http_handler(handler))
    return srv
  end

  ------------------------------------------------------------------------------
  -- HTTP server methods
  ------------------------------------------------------------------------------
  http = {
    createServer = createServer,
  }
end

return http
