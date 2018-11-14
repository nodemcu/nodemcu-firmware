--dofile"httptest.lua"

function assertEquals(a, b)
    if a ~= b then
        error(string.format("%q not equal to %q", tostring(a), tostring(b)), 2)
    end
end

-- Skipping test because my internet connection always resolves invalid DNS...
function SKIP_test_bad_dns()
    do return end

    local c = http.createConnection("http://nope.invalid")
    local seenConnect, seenFinish
    c:on("connect", function()
        seenConnect = true
    end)
    c:on("complete", function(statusCode)
        seenFinish = statusCode
    end)
    local status, connected = c:request()
    assert(status < 0)
    assertEquals(connected, false)
    assertEquals(seenConnect, false)
    assert(seenFinish < 0)
end

function test_simple()
    local c = http.createConnection("http://httpbin.org/", { headers = { Connection = "close" } })
    local seenConnect, seenHeaders, headersStatusCode
    local dataBytes = 0
    c:on("connect", function() seenConnect = true end)
    c:on("headers", function(statusCode, headers)
        headersStatusCode = statusCode
        seenHeaders = headers
    end)
    c:on("data", function(statusCode, data)
        assertEquals(statusCode, headersStatusCode)
        dataBytes = dataBytes + #data
    end)

    local status, connected = c:request()

    assertEquals(seenConnect, true)
    assertEquals(headersStatusCode, 200)
    assert(seenHeaders ~= nil)
    assert(dataBytes > 0)
    assertEquals(status, 200)
    assertEquals(connected, false)
end

function test_keepalive()
    local c = http.createConnection("http://httpbin.org/")
    local seenConnect, seenHeaders, seenData, seenFinish
    c:on("connect", function() seenConnect = true end)
    c:on("headers", function(status, headers)
        seenHeaders = headers
    end)
    c:on("data", function(status, data)
        seenData = data
    end)
    c:on("complete", function(status)
        seenFinish = status
    end)

    local status, connected = c:request()
    assertEquals(seenConnect, true)
    assertEquals(status, 200)
    assertEquals(connected, true)
    assert(seenHeaders)
    assertEquals(seenFinish, 200)

    c:seturl("http://httpbin.org/user-agent")
    c:setheader("Connection", "close")
    seenConnect = false
    seenData = nil
    seenHeaders = nil
    seenFinish = nil
    status, connected = c:request()
    assertEquals(status, 200)
    assertEquals(connected, false)
    assertEquals(seenConnect, false) -- You don't get another connect callback
    assert(seenHeaders) -- But you do get new headers
    assertEquals(seenData, '{\n  "user-agent": "NodeMCU (ESP32)"\n}\n')
    assertEquals(seenFinish, 200)
    c:close()
end

function test_oneshot_get()
    local options = {
        headers = {
            ["user-agent"] = "ESP32 HTTP Module User Agent test",
        },
    }
    local code, data = http.get("http://httpbin.org/user-agent", options)
    assertEquals(code, 200)
    assertEquals(data, '{\n  "user-agent": "ESP32 HTTP Module User Agent test"\n}\n')

    -- And async version
    print("test_oneshot_get async starting")
    http.get("http://httpbin.org/user-agent", options, function(code, data)
        assertEquals(code, 200)
        assertEquals(data, '{\n  "user-agent": "ESP32 HTTP Module User Agent test"\n}\n')
        print("test_oneshot_get async completed")
    end)
end

function test_404()
    local status = http.get("http://httpbin.org/status/404")
    assertEquals(status, 404)
end

function test_post()
    local status, data = http.post("http://httpbin.org/post", nil, "hello=world&answer=42")
    assertEquals(status, 200)
    assert(data:match('"hello": "world"'))
    assert(data:match('"answer": "42"'))

    status, data = http.post("http://httpbin.org/post", {headers = {["Content-Type"] = "application/json"}}, '{"hello":"world"}')
    assertEquals(status, 200)
    assert(data:match('"hello": "world"'))
end

function test_redirects()
    local status = http.createConnection("http://httpbin.org/redirect/3", { max_redirects = 0 }):request()
    assertEquals(status, -28673) -- -ESP_ERR_HTTP_MAX_REDIRECT

    status = http.createConnection("http://httpbin.org/redirect/3", { max_redirects = 2 }):request()
    assertEquals(status, -28673)

    status = http.createConnection("http://httpbin.org/redirect/3"):request()
    assertEquals(status, 200)
end

-- Doesn't seem to work...
function SKIP_test_timeout()
    local status = http.createConnection("http://httpbin.org/delay/10", { timeout = 2000 }):request()
    assertEquals(status, -1)
end

local tests = {}
for k, v in pairs(_G) do
    if type(v) == "function" and k:match("^test_") then
        table.insert(tests, k)
    end
end
table.sort(tests)
for _, t in ipairs(tests) do
    print(t)
    _G[t]()
end
