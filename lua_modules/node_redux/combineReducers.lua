local require, type, error, pairs, table, tostring = require, type, error, pairs, table, tostring

local ActionTypes = require('actionType_utils')
local isPlainObject = require('isPlainObject_utils')


local function _getKeys(object)
    local keyset={}
    local n = 0
    for k, _ in pairs(object) do
        n = n + 1
        keyset[n] = k
    end
    return keyset
end

local function _getKeysString(object)
    local keys = ''
    for k, _ in pairs(object) do
        keys = tostring(k) .. ', ' .. keys
    end
    return string.sub(keys, 1, -3)
end

local function getUnexpectedStateShapeWarningMessage(inputState, reducers, action, unexpectedKeyCache)
    local reducerKeys = _getKeys(reducers)
    local argumentName = 'previous state received by the reducer'
    if(action ~= nil and action.type == ActionTypes.INIT) then
        argumentName = 'preloadedState argument passed to createStore'
    end

    if(#reducerKeys == 0) then
        return 'Store does not have a valid reducer. Make sure the argument passed ' ..
                'to combineReducers is an object whose values are reducers.'
    end

    if (not isPlainObject(inputState)) then
        local keys = _getKeysString(reducers)
        return 'The' .. argumentName .. 'has unexpected type. ' ..
                'Expected argument to be an object with the following ' ..
                'keys "' .. keys .. '"'
    end

    local unexpectedKeys = {}
    for k, _ in pairs(inputState) do
        if(reducers[k] == nil and not unexpectedKeyCache[k]) then
            table.insert(unexpectedKeys, k)
        end
    end

    for _, v in pairs(unexpectedKeys) do
        unexpectedKeyCache[v] = true
    end

    if (action ~= nil and action.type == ActionTypes.REPLACE) then
        return
    end

    if (#unexpectedKeys > 0) then
        return "Unexpected " .. #unexpectedKeys > 1 and "'keys'" or "'key'" ..
                '"'.. _getKeysString(unexpectedKeys) ..'" found in ' .. argumentName .. '.' ..
                'Expected to find one of the known reducer keys instead: "' ..
                _getKeysString(unexpectedKeys) ..'". Unexpected keys will be ignored.'
    end
end

local function assertReducerShape(reducers)
    for k, v in pairs(reducers) do
        local reducer = v
        local initialState = reducer(nil, { type = ActionTypes.INIT })

        if (type(initialState) == 'nil') then
            error(
                'The slice reducer for key "' .. k .. '" returned undefined during initialization. ' ..
                'If the state passed to the reducer is undefined, you must '..
                'explicitly return the initial state. The initial state may '..
                "not be undefined. If you don't want to set a value for this reducer, "..
                "you can use null instead of undefined.",
                2
            )
        end

        initialState = reducer(nil, { type = ActionTypes.PROBE_UNKNOWN_ACTION() })

        if (type(initialState) == 'nil') then
            error(
                'The slice reducer for key "' .. k .. '" returned undefined when probed with a random type. ' ..
                        "Don't try to handle '" .. ActionTypes.INIT .. "' or other actions in \"redux/*\" " ..
                        'namespace. They are considered private. Instead, you must return the ' ..
                        'current state for any unknown actions, unless it is undefined, ' ..
                        'in which case you must return the initial state, regardless of the ' ..
                        'action type. The initial state may not be undefined, but can be null.',
                2
            )
        end
    end
end

local function combineReducers(reducers)
    local reducerKeys = _getKeys(reducers)
    local finalReducers = {}

    for _, v in pairs(reducerKeys) do
        if (type(reducers[v]) == 'function' ) then
            finalReducers[v] = reducers[v]
        end
    end

    local finalReducerKeys = _getKeys(finalReducers)


    local unexpectedKeyCache = {}
    local shapeAssertionError
    local res, val = pcall(assertReducerShape, finalReducers)

    if(not res) then
        shapeAssertionError = val
    end

    local function combination(state, action)
        if (shapeAssertionError) then
            error (shapeAssertionError, 2)
        end

        state = state or {}

        local warningMessage = getUnexpectedStateShapeWarningMessage(
            state,
            finalReducers,
            action,
            unexpectedKeyCache
        )

        if (warningMessage) then
            print('\27[93mWARNING :: '..warningMessage..'\27[0m')
        end

        local hasChanged = false
        local nextState = {}


        for _, v in pairs(finalReducerKeys) do
            local key = v
            local reducer = finalReducers[key]
            local previousStateForKey = state[key]
            local nextStateForKey = reducer(previousStateForKey, action)

            if(type(nextStateForKey) == 'nil') then
                local actionType = action and action.type
                error(
                    'When called with an action of type "'.. actionType or '(unknown type)' .. '"' ..
                    'the slice reducer for key "' .. key .. '" returned undefined. ' ..
                    'To ignore an action, you must explicitly return the previous state. ' ..
                    'If you want this reducer to hold no value, you can return null instead of undefined.',
                    2
                )
            end
            nextState[key] = nextStateForKey
            hasChanged = hasChanged or nextStateForKey ~= previousStateForKey
        end
        hasChanged = hasChanged or #finalReducerKeys ~= #_getKeys(state)

        if (hasChanged) then
            return nextState
        else
            return state
        end
    end
    return combination
end

return combineReducers
