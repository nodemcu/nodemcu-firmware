--dofile"httptest.lua"

function assertEquals(a, b)
    if a ~= b then
        error(string.format("%q not equal to %q", tostring(a), tostring(b)), 2)
    end
end

function test_bad_dns()
    local c = http.createConnection("http://nope.invalid")
    local seenConnect, seenFinish
    c:on("connect", function()
        seenConnect = true
    end)
    c:on("finish", function(statusCode)
        seenFinish = statusCode
    end)
    local connected = c:request()
    assertEquals(connected, false)
    assertEquals(seenConnect, false)
    assertEquals(seenFinish, -1)
end

function test_simple()
    local c = http.createConnection("http://httpbin.org/", { Connection = "close" })
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

    local connected = c:request()

    assertEquals(seenConnect, true)
    assertEquals(headersStatusCode, 200)
    assert(seenHeaders ~= nil)
    assert(dataBytes > 0)
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
    c:on("finish", function(status)
        seenFinish = status
    end)

    local connected = c:request()
    assertEquals(seenConnect, true)
    assertEquals(connected, true)
    assert(seenHeaders)
    assertEquals(seenFinish, 200)

    c:seturl("http://httpbin.org/user-agent")
    c:setheader("Connection", "close")
    seenConnect = false
    seenData = nil
    seenHeaders = nil
    seenFinish = nil
    connected = c:request()
    assertEquals(connected, false)
    assertEquals(seenConnect, false) -- You don't get another connect callback
    assert(seenHeaders) -- But you do get new headers
    assertEquals(seenData, '{\n  "user-agent": "ESP32 HTTP Client/1.0"\n}\n')
    assertEquals(seenFinish, 200)
    c:close()
end

function test_oneshot_get()
    local headers = { ["user-agent"] = "ESP32 HTTP Module User Agent test" }
    local seenCode, seenData
    http.get("http://httpbin.org/user-agent", headers, function(status, data)
        seenCode = status
        seenData = data
    end)
    assertEquals(seenCode, 200)
    assertEquals(seenData, '{\n  "user-agent": "ESP32 HTTP Module User Agent test"\n}\n')
end

function test_404()
    local seenStatus
    http.get("http://httpbin.org/status/404", function(status)
        seenStatus = status
    end)
    assertEquals(seenStatus, 404)
end

function test_post()
    local seenStatus, seenData
    http.post("http://httpbin.org/post", nil, "hello=world&answer=42", function(status, data)
        seenStatus = status
        seenData = data
    end)

    assertEquals(seenStatus, 200)
    assert(seenData:match('"hello": "world"'))
    assert(seenData:match('"answer": "42"'))

    seenStatus = nil
    seenData = nil
    http.post("http://httpbin.org/post", {["Content-Type"] = "application/json"}, '{"hello":"world"}', function(status, data)
        seenStatus = status
        seenData = data
    end)
    assertEquals(seenStatus, 200)
    assert(seenData:match('"hello": "world"'))
end

for _, t in ipairs{--[["test_bad_dns",]] "test_simple", "test_keepalive", "test_oneshot_get", "test_404", "test_post"} do
    print(t)
    _G[t]()
end
