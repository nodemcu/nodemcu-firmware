-- Somfy module example (beside somfy module requires also SJSON module)
-- The rolling code number is stored in the file somfy.cfg. A cached write of the somfy.cfg file is implemented in order to reduce the number of write to the EEPROM memory. Together with the logic of the file module it should allow long lasting operation.

config_file = "somfy."
-- somfy.cfg looks like
-- {"window1":{"rc":1,"address":123},"window2":{"rc":1,"address":124}}

local tmr_cache = tmr.create()
local tmr_delay = tmr.create()

pin = 4
gpio.mode(pin, gpio.OUTPUT, gpio.PULLUP)

function deepcopy(orig)
    local orig_type = type(orig)
    local copy
    if orig_type == 'table' then
        copy = {}
        for orig_key, orig_value in next, orig, nil do
            copy[deepcopy(orig_key)] = deepcopy(orig_value)
        end
        setmetatable(copy, deepcopy(getmetatable(orig)))
    else -- number, string, boolean, etc
        copy = orig
    end
    return copy
end

function readconfig()
    local cfg, ok, ln
    if file.exists(config_file.."cfg") then
        print("Reading config from "..config_file.."cfg")
        file.open(config_file.."cfg", "r+")
        ln = file.readline()
        file.close()
    else
        if file.exists(config_file.."bak") then
            print("Reading config from "..config_file.."bak")
            file.open(config_file.."bak", "r+")
            ln = file.readline()
            file.close()
        end
    end
    if not ln then ln = "{}" end
    print("Configuration: "..ln)
    config = sjson.decode(ln)
    config_saved = deepcopy(config)
end

function writeconfighard()
    print("Saving config")
    file.remove(config_file.."bak")
    file.rename(config_file.."cfg", config_file.."bak")
    file.open(config_file.."cfg", "w+")
    local ok, cfg = pcall(sjson.encode, config)
    if ok then
        file.writeline(cfg)
    else
        print("Config not saved!")
    end
    file.close()

    config_saved = deepcopy(config)
end

function writeconfig()
    tmr.stop(tmr_cache)
    local savenow = false
    local savelater = false

--print("Config: "..sjson.encode(config))
--print("Config saved: "..sjson.encode(config))
 
    local count = 0
    for _ in pairs(config_saved) do count = count + 1 end
    if count == 0 then
        config_saved = readconfig()
    end
    for remote,cfg in pairs(config_saved) do
        savelater = savelater or not config[remote] or config[remote].rc > cfg.rc
        savenow = savenow or not config[remote] or config[remote].rc > cfg.rc + 10
    end
    savelater = savelater and not savenow
    if savenow then
        print("Saving config now!")
        writeconfighard()
    end
    if savelater then
        print("Saving config later")
        tmr.alarm(tmr_cache, 65000, tmr.ALARM_SINGLE, writeconfighard)
    end
end

--======================================================================================================--
function down(remote, cb, par)
    par = par or {}
    print("down: ".. remote)
    config[remote].rc=config[remote].rc+1
    somfy.sendcommand(pin, config[remote].address, somfy.DOWN, config[remote].rc, 16, function() wait(100, cb, par) end)
    writeconfig()
end

function up(remote, cb, par)
    par = par or {}
    print("up: ".. remote)
    config[remote].rc=config[remote].rc+1
    somfy.sendcommand(pin, config[remote].address, somfy.UP, config[remote].rc, 16, function() wait(100, cb, par) end)
    writeconfig()
end

function downStep(remote, cb, par)
    par = par or {}
    print("downStep: ".. remote)
    config[remote].rc=config[remote].rc+1
    somfy.sendcommand(pin, config[remote].address, somfy.DOWN, config[remote].rc, 2,  function() wait(300, cb, par) end)
    writeconfig()
end

function upStep(remote, cb, par)
    par = par or {}
    print("upStep: ".. remote)
    config[remote].rc=config[remote].rc+1
    somfy.sendcommand(pin, config[remote].address, somfy.UP, config[remote].rc, 2, function() wait(300, cb, par) end)
    writeconfig()
end

function wait(ms, cb, par)
    par = par or {}
    print("wait: ".. ms)
    if cb then tmr.alarm(tmr_delay, ms, tmr.ALARM_SINGLE, function () cb(unpack(par)) end) end
end


--======================================================================================================--
if not config then readconfig() end
if #config == 0 then -- somfy.cfg does not exist
    config = sjson.decode([[{"window1":{"rc":1,"address":123},"window2":{"rc":1,"address":124}}]])
    config_saved = deepcopy(config)
end
down('window1', 
    wait, {60000, 
    up, {'window1', 
    wait, {9000, 
    downStep, {'window1', downStep, {'window1', downStep, {'window1', downStep, {'window1', downStep, {'window1', downStep, {'window1', downStep, {'window1'
}}}}}}}}}})
