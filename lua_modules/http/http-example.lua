------------------------------------------------------------------------------
-- HTTP server Hello world example
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
------------------------------------------------------------------------------
html = {"<h1>", "hello nodemcu", "</h1>"}

require("httpserver").createServer(80, function(req, res)
  -- analyse method and url
  print("+R", req.method, req.url, node.heap())
  -- setup handler of headers, if any
  req.onheader = function(self, name, value)
    print("+H", name, value)
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
  req.ondata = function(self, chunk, last)
    print("+B", chunk and #chunk, node.heap())
    if last then
      -- reply
      local headers = {}
      headers.Connection = "close"
      headers["Content-Type"] = "text/html; charset=utf-8"
      res.send_header({headers = headers})
      for i = 1, #html do res.send(html[i]) end
      res.send() -- don't forget this line
    end
  end
end)
