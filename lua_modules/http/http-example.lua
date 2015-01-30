------------------------------------------------------------------------------
-- HTTP server Hello world example
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
------------------------------------------------------------------------------
require("http").createServer(80, function(req, res)
  -- analyse method and url
  print("+R", req.method, req.url, node.heap())
  -- setup handler of headers, if any
  req.onheader = function(self, name, value)
    -- print("+H", name, value)
    -- E.g. look for "content-type" header,
    --   setup body parser to particular format
    -- if name == "content-type" then
    --   if value == "application/json" then
    --     req.ondata = function(self, chunk) ... end
    --   elseif value == "application/x-www-form-urlencoded" then
    --     req.ondata = function(self, chunk) ... end
    --   end
    -- end
  end
  -- setup handler of body, if any
  req.ondata = function(self, chunk)
    print("+B", chunk and #chunk, node.heap())
    -- request ended?
    if not chunk then
      -- reply
      --res:finish("")
      res:send(nil, 200)
      res:send_header("Connection", "close")
      res:send("Hello, world!")
      res:finish()
    end
  end
  -- or just do something not waiting till body (if any) comes
  --res:finish("Hello, world!")
  --res:finish("Salut, monde!")
end)
