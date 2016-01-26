-- Support HTTP and HTTPS, For example
-- HTTP POST Example with JSON header and body
http.post("http://somewhere.acceptjson.com/",
    "Content-Type: application/json\r\n",
    "{\"hello\":\"world\"}",
    function(code, data)
        print(code)
        print(data)
    end)

-- HTTPS GET Example with NULL header
http.get("https://www.vowstar.com/nodemcu/","",
    function(code, data)
        print(code)
        print(data)
    end)
-- You will get
-- > 200
-- hello nodemcu

-- HTTPS DELETE Example with NULL header and body
http.delete("https://10.0.0.2:443","","",
    function(code, data)
        print(code)
        print(data)
    end)

-- HTTPS PUT Example with NULL header and body
http.put("https://testput.somewhere/somewhereyouput.php","","",
    function(code, data)
        print(code)
        print(data)
    end)

-- HTTP RAW Request Example, use more HTTP/HTTPS request method
http.request("http://www.apple.com:80/library/test/success.html","GET","","",
    function(code, data)
        print(code)
        print(data)
    end)
