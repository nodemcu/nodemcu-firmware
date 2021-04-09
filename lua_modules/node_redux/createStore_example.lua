--
-- Created by IntelliJ IDEA.
-- User: shubhams
-- Date: 06/04/21
-- Time: 1:53 PM
-- To change this template use File | Settings | File Templates.
--

local redux = require('node_redux')

local function dump(o)
    if type(o) == 'table' then
        local s = '{ '
        for k,v in pairs(o) do
            s = s .. ' ' .. k .. ' = ' .. dump(v) .. ','
        end
        s = string.sub(s, 1, -2)
        return s .. ' }'
    else
        return tostring(o)
    end
end

local function reducer1(state, action)
    state = state or {
        value = {0}
    }
    if action.type == 'counter/incremented' then
        return {
            value = {state.value[1] + 1}
        }
    elseif action.type ==  'counter/decremented' then
        return {
            value =  {state.value[1] - 2}
        }
    else
        return state
    end
end

local function reducer2(state, action)
    state = state or {
        value = 0
    }
    if action.type == 'counter/incremented' then
        return {
            value = state.value + 2
        }
    elseif action.type ==  'counter/decremented' then
        return {
            value = state.value - 1
        }
    else
        return state
    end
end

local newReducer = redux.combineReducers({
    r1 = reducer1,
    r2 = reducer2
})


redux.createStore(newReducer)


local function s(pState, cState)
    print('Previous State: ' .. dump(pState), '\nCurrent State: ' .. dump(cState) .. '\n\n')
end


redux.store.subscribe(s)

redux.store.dispatch({type = 'counter/incremented'})
redux.store.dispatch({type = 'counter/decremented'})
redux.store.dispatch({type = 'counter/incremented'})
