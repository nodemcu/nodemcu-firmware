# NodeRedux 
Redux is a predictable state container for lua apps and NodeMCU.

## Influences

NodeRedux evolves the ideas of [Redux](https://redux.js.org/), 
which help to maintain state lua base app basically NodeMCU devices.


## Basic Example

The whole global state of your app is stored in an object tree inside a single _store_.
The only way to change the state tree is to create an _action_, 
an object describing what happened, and _dispatch_ it to the store.
To specify how state gets updated in response to an action, you write pure _reducer_
functions that calculate a new state based on the old state and the action.


```lua
local redux = require('redux') -- optional line for nodemcu, only need for lua app

-- This is a reducer - a function that takes a current state value and an
-- action object describing "what happened", and returns a new state value.
-- A reducer's function signature is: (state, action) => newState
--
-- The NodeRedux state should contain only plain Lua table.
-- The root state value is usually an table.  It's important that you should
-- not mutate the state table, but return a new table if the state changes.
--
-- You can use any conditional logic you want in a reducer. In this example,
-- we use a if statement, but it's not required.

local function counterReducer(state, action)
    state = state or { value = 0 }
    if action.type == 'counter/incremented' then
        return { value = state.value + 2 }
    elseif action.type ==  'counter/decremented' then
        return { value = state.value - 1 }
    else
        return state
    end
end

redux.createStore(counterReducer)

local function console(pState, cState)
    print('Previous State: '.. pState.value, 'Current State: '.. cState.value)
end

redux.store.subscribe(console)

redux.store.dispatch({type = 'counter/incremented'})
-- {value = 1}
redux.store.dispatch({type = 'counter/incremented'})
-- {value = 2}
redux.store.dispatch({type = 'counter/decremented'})
-- {value = 1}
```

[Here](./createStore_example.lua) the example code

## License

[MIT](LICENSE)


