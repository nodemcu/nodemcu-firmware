local pairs, type = pairs, type

local function _checkObect(obj)
    local check = true
    for _, v in pairs(obj) do
        local obType = type(v)
        if (obType == 'string' or obType == 'number' or obType == 'nil' or obType == 'boolean') then
            check = check and true
        elseif(obType == 'table') then
            check = check and _checkObect(v)
        else
            check = false
            break
        end
    end
    return check
end

local isPlainObject = function (obj)
    if(type(obj) ~= 'table' or obj == nil) then
        return false
    end
    return _checkObect(obj)
end


return isPlainObject
