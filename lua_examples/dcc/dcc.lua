-- Simple example for responding to NMRA DCC commands
-- author @voborsky
local PIN = 2 -- GPIO4

local addr = 0x12a

local CV = {[29]=0,
      [1]=bit.band(addr, 0x3f), --CV_ACCESSORY_DECODER_ADDRESS_LSB (6 bits)
      [9]=bit.band(bit.rshift(addr,6), 0x7)  --CV_ACCESSORY_DECODER_ADDRESS_MSB (3 bits)
     }

local function deepcopy(orig)
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

local cmd_last
local params_last

local function is_new(cmd, params)
    if cmd ~= cmd_last then return true end
    for i,j in pairs(params) do
        if params_last[i] ~= j then return true end
    end
    return false
end

local function DCC_command(cmd, params)
    if not is_new(cmd, params) then return end
    if cmd == dcc.DCC_IDLE then
        return
    elseif cmd == dcc.DCC_TURNOUT then
        print("Turnout command")
    elseif cmd == dcc.DCC_SPEED then
        print("Speed command")
    elseif cmd == dcc.DCC_FUNC then
        print("Function command")
    else
        print("Other command", cmd)
    end

    for i,j in pairs(params) do
        print(i, j)
    end
    print(("="):rep(80))
    cmd_last = cmd
    params_last = deepcopy(params)
end

local function CV_callback(operation, param)
    local oper = ""
    local result
    if operation == dcc.CV_WRITE then
        oper = "Write"
        CV[param.CV]=param.Value
    elseif operation == dcc.CV_READ then
        oper = "Read"
        result = CV[param.CV]
    elseif operation == dcc.CV_VALID then
        oper = "Valid"
        result = 1
    elseif operation == dcc.CV_RESET then
        oper = "Reset"
        CV = {}
    end
    print(("[CV_callback] %s CV %d%s")
      :format(oper, param.CV, param.Value and "\tValue: "..param.Value or "\tValue: nil"))
    return result
end

dcc.setup(PIN,
    DCC_command,
    dcc.MAN_ID_DIY, 1,
    -- Accessories (turnouts) decoder:
    --bit.bor(dcc.FLAGS_AUTO_FACTORY_DEFAULT, dcc.FLAGS_DCC_ACCESSORY_DECODER, dcc.FLAGS_MY_ADDRESS_ONLY),
    -- Cab (train) decoder
    bit.bor(dcc.FLAGS_AUTO_FACTORY_DEFAULT),
    0, -- ???
    CV_callback)
