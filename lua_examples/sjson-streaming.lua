-- Test sjson and GitHub API

local s = tls.createConnection()
s:on("connection", function(sck, c)
  sck:send("GET /repos/nodemcu/nodemcu-firmware/git/trees/master HTTP/1.0\r\nUser-agent: nodemcu/0.1\r\nHost: api.github.com\r\nConnection: close\r\nAccept: application/json\r\n\r\n")
end)

function startswith(String, Start)
  return string.sub(String, 1, string.len(Start)) == Start
end

local seenBlank = false
local partial
local wantval = { tree = 1, path = 1, url = 1 }
-- Make an sjson decoder that only keeps certain fields
local decoder = sjson.decoder({
  metatable =
  {
    __newindex = function(t, k, v)
      if wantval[k] or type(k) == "number" then
        rawset(t, k, v)
      end
    end
  }
})
local function handledata(s)
  decoder:write(s)
end

-- The receive callback is somewhat gnarly as it has to deal with find the end of the header
-- and having the newline sequence split across packets
s:on("receive", function(sck, c)
  if partial then
    c = partial .. c
    partial = nil
  end
  if seenBlank then
    handledata(c)
    return
  end
  while c do
    if startswith(c, "\r\n") then
      seenBlank = true
      c = c:sub(3)
      handledata(c)
      return
    end
    local s, e = c:find("\r\n")
    if s then
      -- Throw away line
      c = c:sub(e + 1)
    else
      partial = c
      c = nil
    end
  end
end)

local function getresult()
  local result = decoder:result()
  -- This gets the resulting decoded object with only the required fields
  print(result['tree'][4]['path'], "is at",
    result['tree'][4]['url'])
end

s:on("disconnection", getresult)
s:on("reconnection", getresult)

-- Make it all happen!
s:connect(443, "api.github.com")
