-- a simple telnet server

telnet_srv = net.createServer(net.TCP, 180)
telnet_srv:listen(2323, function(socket)
    local fifo = {}
    local fifo_drained = true

    local function sender(c)
        if #fifo > 0 then
            c:send(table.remove(fifo, 1))
        else
            fifo_drained = true
        end
    end

    local function s_output(str)
        table.insert(fifo, str)
        if socket ~= nil and fifo_drained then
            fifo_drained = false
            sender(socket)
        end
    end

    node.output(s_output, 0)   -- re-direct output to function s_ouput.

    socket:on("receive", function(c, l)
        node.input(l)           -- works like pcall(loadstring(l)) but support multiple separate line
    end)
    socket:on("disconnection", function(c)
        node.output(nil)        -- un-regist the redirect output function, output goes to serial
    end)
    socket:on("sent", sender)

    print("Welcome to NodeMcu world.")
end)
