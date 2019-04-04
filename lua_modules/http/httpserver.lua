------------------------------------------------------------------------------
-- HTTP server module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
------------------------------------------------------------------------------
local collectgarbage, tonumber, tostring = collectgarbage, tonumber, tostring

local http
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
   local send = function(self, data, status)
    -- TODO: req.send should take care of response headers!
    if self.send_header then
      csend("HTTP/1.1 ")
      csend(tostring(status or 200))
      -- TODO: real HTTP status code/name table
      csend(" OK\r\n")
      -- we use chunked transfer encoding, to not deal with Content-Length:
      --   response header
      self:send_header("Transfer-Encoding", "chunked")
      -- TODO: send standard response headers, such as Server:, Date:
    end
    if data then
      -- NB: no headers allowed after response body started
      if self.send_header then
        self.send_header = nil
        -- end response headers
        csend("\r\n")
      end
      -- chunked transfer encoding
      csend(("%X\r\n"):format(#data))
      csend(data)
      csend("\r\n")
    end
   end
   local send_header = function(self, name, value)
    -- NB: quite a naive implementation
    csend(name)
    csend(": ")
    csend(value)
    csend("\r\n")
   end
   -- finalize request, optionally sending data
   local finish = function(self, data, status)
    -- NB: res.send takes care of response headers
    if data then
      self:send(data, status)
    end
    -- finalize chunked transfer encoding
    csend("0\r\n\r\n")
    -- close connection
    cfini()
   end
   --
    local res = { }
    res.send_header = send_header
    res.send = send
    res.finish = finish
    return res
  end

  ------------------------------------------------------------------------------
  -- HTTP parser
  ------------------------------------------------------------------------------
  local http_handler = function(handler)
    return function(conn)
      local csend = (require "fifosock").wrap(conn)
      local cfini = function()
        conn:on("receive", nil)
        conn:on("disconnection", nil)
        csend(function() conn:on("sent", nil) conn:close() end)
      end
      local req, res
      local buf = ""
      local method, url
      local ondisconnect = function(conn)
	conn.on("sent", nil)
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
        req:ondata(chunk)
        -- NB: once length of seen chunks equals Content-Length:
        --   onend(conn) is called
        body_len = body_len + #chunk
        -- print("-B", #chunk, body_len, cnt_len, node.heap())
        if body_len >= cnt_len then
          req:ondata()
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
