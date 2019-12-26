local moduleName = ... or 'mispec'
local M = {}
_G[moduleName] = M

-- Helpers:
function ok(expression, desc)
    if expression == nil then expression = false end
    desc = desc or 'expression is not ok'
    if not expression then
        error(desc .. '\n' .. debug.traceback())
    end
end

function ko(expression, desc)
    if expression == nil then expression = false end
    desc = desc or 'expression is not ko'
    if expression then
        error(desc .. '\n' .. debug.traceback())
    end
end

function eq(a, b)
    if type(a) ~= type(b) then
        error('type ' .. type(a) .. ' is not equal to ' .. type(b) .. '\n' .. debug.traceback())
    end
    if type(a) == 'function' then
        return string.dump(a) == string.dump(b)
    end
    if a == b then return true end
    if type(a) ~= 'table' then
        error(string.format("%q",tostring(a)) .. ' is not equal to ' .. string.format("%q",tostring(b)) .. '\n' .. debug.traceback())
    end
    for k,v in pairs(a) do
        if b[k] == nil or not eq(v, b[k]) then return false end
    end
    for k,v in pairs(b) do
        if a[k] == nil or not eq(v, a[k]) then return false end
    end
    return true
end

function failwith(message, func, ...)
    local status, err = pcall(func, ...)
    if status then
        local messagePart = ""
        if message then
            messagePart = " containing \"" .. message .. "\""
        end
        error("Error expected" .. messagePart .. '\n' .. debug.traceback())
    end
    if (message and not string.find(err, message)) then
        error("expected errormessage \"" .. err .. "\" to contain \"" .. message .. "\"" .. '\n' .. debug.traceback() )
    end
    return true
end

function fail(func, ...)
    return failwith(nil, func, ...)
end

local function eventuallyImpl(func, retries, delayMs)
    local prevEventually = _G.eventually
    _G.eventually = function() error("Cannot nest eventually/andThen.") end
    local status, err = pcall(func)
    _G.eventually = prevEventually
    if status then
        M.queuedEventuallyCount = M.queuedEventuallyCount - 1
        M.runNextPending()
    else
        if retries > 0 then
            local t = tmr.create()
            t:register(delayMs, tmr.ALARM_SINGLE, M.runNextPending)
            t:start()

            table.insert(M.pending, 1, function() eventuallyImpl(func, retries - 1, delayMs) end)
        else
            M.failed = M.failed + 1
            print("\n  ! it failed:", err)

            -- remove all pending eventuallies as spec has failed at this point
            for i = 1, M.queuedEventuallyCount - 1 do
                table.remove(M.pending, 1)
            end
            M.queuedEventuallyCount = 0
            M.runNextPending()
        end
    end
end

function eventually(func, retries, delayMs)
    retries = retries or 10
    delayMs = delayMs or 300

    M.queuedEventuallyCount = M.queuedEventuallyCount + 1

    table.insert(M.pending, M.queuedEventuallyCount, function()
        eventuallyImpl(func, retries, delayMs)
    end)
end

function andThen(func)
    eventually(func, 0, 0)
end

function describe(name, itshoulds)
    M.name = name
    M.itshoulds = itshoulds
end

-- Module:
M.runNextPending = function()
    local next = table.remove(M.pending, 1)
    if next then
        node.task.post(next)
        next = nil
    else
        M.succeeded = M.total - M.failed
        local elapsedSeconds = (tmr.now() - M.startTime) / 1000 / 1000
        print(string.format(
            '\n\nCompleted in %d seconds; %d failed out of %d.',
            elapsedSeconds, M.failed, M.total))
        M.pending = nil
        M.queuedEventuallyCount = nil
    end
end

M.run = function()
    M.pending = {}
    M.queuedEventuallyCount = 0
    M.startTime = tmr.now()
    M.total = 0
    M.failed = 0
    local it = {}
    it.should = function(_, desc, func)
        table.insert(M.pending, function()
            print('\n  * ' .. desc)
            M.total = M.total + 1
            if M.pre then M.pre() end
            local status, err = pcall(func)
            if not status then
                print("\n  ! it failed:", err)
                M.failed = M.failed + 1
            end
            if M.post then M.post() end
            M.runNextPending()
        end)
    end
    it.initialize = function(_, pre) M.pre = pre end;
    it.cleanup = function(_, post) M.post = post end;
    M.itshoulds(it)

    print('' .. M.name .. ', it should:')
    M.runNextPending()

    M.itshoulds = nil
    M.name = nil
end

print ("loaded mispec")
