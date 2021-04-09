local ActionType
do
    local upperCase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    local lowerCase = "abcdefghijklmnopqrstuvwxyz"
    local numbers = "0123456789"

    local characterSet = upperCase .. lowerCase .. numbers

    local function randomeSting(length)
        local output = ""
        for	_ = 1, length do
            output = math.random(#characterSet) .. output
        end
        return output
    end

    local intString = randomeSting(36)
    local INIT = "@redux/INIT" .. intString

    local function PROBE_UNKNOWN_ACTION()
        return '@@redux/PROBE_UNKNOWN_ACTION' .. randomeSting(36)
    end


    ActionType = {
        INIT = INIT,
        PROBE_UNKNOWN_ACTION = PROBE_UNKNOWN_ACTION
    }
end

return ActionType
