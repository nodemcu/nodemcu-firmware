# NodeRedux Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2021-04-09 | [Shubham Srivastava](https://github.com/shubham-sri) | [Shubham Srivastava](https://github.com/shubham-sri) | [node_redux.lua](../../lua_modules/node_redux/node_redux.lua) |

<br>
Redux is a predictable state container for lua apps and NodeMCU.

### Influences

NodeRedux evolves the ideas of [Redux](https://redux.js.org/),
which help to maintain state lua base app basically NodeMCU devices.

### Basic Example

The whole global state of your app is stored in an object tree inside a single _store_.
The only way to change the state tree is to create an _action_,
an object describing what happened, and _dispatch_ it to the store.
To specify how state gets updated in response to an action, you write pure _reducer_
functions that calculate a new state based on the old state and the action.


```lua
local redux = require('redux')

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

[Here](../../lua_modules/node_redux/createStore_example.lua) the example code

### Require
```lua
local redux = require("node_redux")
```

## redux.createStore()
Creates a Redux store that holds the complete state tree of device. There should only be a single store in your device.

#### Syntax
`redux.createStore(reducer)`

#### Parameters
- `reducer`(Function): A reducing function that returns the next state tree, 
  given the current state tree, and an action to handle.
- `preloadedState`(any): The initial state. You may optionally specify it to 
  hydrate the state from the server in universal apps, or to restore a previously 
  serialized user session. If you produced reducer with combineReducers, this must 
  be a plain object with the same shape as the keys passed to it. Otherwise, 
  you are free to pass anything that your reducer can understand.
- `enhancer` (Function): The store enhancer. You may optionally specify it to 
  enhance the store with third-party capabilities such as middleware, time travel, 
  persistence, etc. The only store enhancer that ships with Redux is 
  applyMiddleware() ***TODO: Support added in near next PR***

#### Returns
- `nil`

## redux.combineReducers()
As your app grows more complex, you'll want to split your reducing function into 
separate functions, each managing independent parts of the state.
The combineReducers helper function turns an object whose values are different 
reducing functions into a single reducing function you can pass to createStore.

#### Syntax
```lua
redux.combineReducers({
    reducer_one = reducer_1,
    reducer_two = reducer_2,
    ...
})
```

#### Parameters
- `table`(Table of Functions values): A reducing functions that returns the next 
  state tree, given the current state tree, and an action to handle.
#### Returns

- `combined_reducer` (Function) A single combined reducer build by combining 
  all reducers

#### Example

```lua
local redux = require('redux')

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

local function inverseCounterReducer(state, action)
    state = state or { value = 0 }
    if action.type == 'counter/incremented' then
        return { value = state.value - 1 }
    elseif action.type ==  'counter/decremented' then
        return { value = state.value + 1 }
    else
        return state
    end
end

local reducer = redux.combineReducers({
    counterReducer = counterReducer,
    inverseCounterReducer = inverseCounterReducer,
})

redux.createStore(reducer)

local function console(pState, cState)
    print('Previous State: counterReducer' .. 
            pState.counterReducer.value, 
        'Current State: counterReducer' .. 
                cState.counterReducer.value
    )
    print('Previous State: inverseCounterReducer' ..
            pState.inverseCounterReducer.value,
        'Current State: inverseCounterReducer' ..
                cState.inverseCounterReducer.value
    )
end

redux.store.subscribe(console)

redux.store.dispatch({type = 'counter/incremented'})
-- {counterReducer = { value = 2 }, inverseCounterReducer = { value = -1 } }
redux.store.dispatch({type = 'counter/incremented'})
-- {counterReducer = { value = 4 }, inverseCounterReducer = { value = -2 } }
redux.store.dispatch({type = 'counter/decremented'})
-- {counterReducer = { value = 3 }, inverseCounterReducer = { value = -1 } }
```

## redux.store.getState()
Returns the current state tree of your application. It is equal to the last value 
returned by the store's reducer.
#### Syntax
`redux.store.getState()`

#### Parameters
None

#### Returns
The current state tree of your application.

## redux.store.dispatch()
Dispatches an action. This is the only way to trigger a state change.

The store's reducing function will be called with the current getState() result,
and the given action synchronously. Its return value will be considered the next state. 
It will be returned from getState() from now on, and the change listeners will 
immediately be notified.
#### Syntax
`redux.store.dispatch(action)`

#### Parameters
- `action` (_Table_): A plain object describing the change that makes sense for 
  your application. Actions must have a type field that indicates the `type` 
  of action being performed. Types can be defined as constants and imported 
  from another module. It's better to use strings for `type` than Symbols 
  because strings are serializable.

#### Returns
(_Table_): The dispatched action

## redux.store.subscribe()
Adds a change listener. It will be called any time an action is dispatched, and some 
part of the state tree may potentially have changed. You may then use `1st arg` for 
previous state and `2nd arg` for current state.

- The listener should only call dispatch() either in response to user actions or 
  under specific conditions (e.g. dispatching an action when the store has 
  a specific field). Calling dispatch() without any conditions is technically 
  possible, however it leads to an infinite loop as every dispatch() call 
  usually triggers the listener again.

#### Syntax
`redux.store.subscribe(listener)`

#### Parameters
- `listener` (Function): The callback to be invoked any time an action has been 
  dispatched, and the state tree might have changed. You may then use `1st arg` for
  previous state and `2nd arg` for current state
  
#### Example - listener
```lua
local function listener(pState, cState)
    print('Previous State: counterReducer' .. 
            pState.counterReducer.value, 
        'Current State: counterReducer' .. 
                cState.counterReducer.value
    )
    print('Previous State: inverseCounterReducer' ..
            pState.inverseCounterReducer.value,
        'Current State: inverseCounterReducer' ..
                cState.inverseCounterReducer.value
    )
end
```

## redux.store.replaceReducer()
It is an advanced API. You might need this if your app implements code 
splitting, and you want to load some of the reducers dynamically. 
You might also need this if you implement a hot reloading mechanism for Redux.

#### Syntax
`redux.store.replaceReducer(nextReducer)`

#### Parameters
- `nextReducer` (Function): The next reducer for the store to use.
