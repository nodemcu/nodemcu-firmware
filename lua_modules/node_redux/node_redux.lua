local require, type, error, ipairs, table, select = require, type, error, ipairs, table, select

local isPlainObject = require('isPlainObject_utils')
local ActionTypes = require('actionType_utils')
local combineReducers = require('combineReducers')

local redux, store
do
    local function createStore(reducer, preloadedState, enhancer, ...)
        if(type(reducer) ~= 'function') then
            error("Expected the root reducer to be a function. Instead, received: "..type(reducer), 2)
        end

        local arg_4 = select(1, ...)

        if((type(preloadedState) == 'function' and type(enhancer) == 'function')
                or (type(enhancer) == 'function' and type(arg_4) == 'function') ) then
            error('It looks like you are passing several store enhancers to ' ..
                'createStore(). This is not supported. Instead, compose them ' ..
                    'together to a single function.', 2)
        end

        if(type(preloadedState) == 'function' and type(enhancer) == 'nil') then
            enhancer = preloadedState
            preloadedState = nil
        end

        if(type(enhancer) ~= 'nil') then
            if(type(enhancer) ~= 'function') then
                error('Expected the enhancer to be a function. Instead, received: ' ..  type(enhancer), 2)
            end

            return enhancer(createStore)(reducer, preloadedState)
        end

        local currentReducer, currentState, currentListeners, nextListeners, isDispatching, countListeners
        do
            currentReducer = reducer
            currentState = preloadedState
            currentListeners = {}
            nextListeners = currentListeners
            isDispatching = false
            countListeners = 0

            local function copy(array)
                if type(array) ~= 'table' then return array end
                local res = {}
                local index = 0
                for _, v in ipairs(array) do
                    res[index] = v
                    index = index + 1
                end
                return res
            end

            local function ensureCanMutateNextListeners()
                if(nextListeners == currentListeners) then
                    nextListeners = copy(currentListeners)
                end
            end

            local function getState()
                if(isDispatching) then
                    error('You may not call store.getState() while the reducer is executing. ' ..
                            'The reducer has already received the state as an argument. ' ..
                            'Pass it down from the top reducer instead of reading it from the store.', 2)
                end

                return currentState
            end

            local function subscribe(listener)
                if(type(listener) ~= 'function') then
                    error('You may not call store.subscribe() while the reducer is executing. ' ..
                            'If you would like to be notified after the store has been updated, subscribe from a ' ..
                            'component and invoke store.getState() in the callback to access the latest state. ', 2)
                end

                local isSubscribed = true
                ensureCanMutateNextListeners()
                table.insert(nextListeners, listener)

                countListeners = countListeners + 1

                local function unsubscribe()
                    if (not isSubscribed) then
                        return
                    end

                    if (isDispatching) then
                        error('You may not unsubscribe from a store listener while the reducer is executing.', 2)
                    end
                    isSubscribed = false
                    ensureCanMutateNextListeners()
                    table.remove(nextListeners, countListeners)
                    currentListeners = nil
                end
                return unsubscribe
            end

            local function dispatch(action)
                if (not isPlainObject(action)) then
                    error("Actions must be plain objects.", 2)
                end

                if (type(action.type) == 'nil') then
                    error('Actions may not have an undefined "type" property. ' ..
                            'You may have misspelled an action type string constant.', 2)
                end

                if(isDispatching) then
                    error('Reducers may not dispatch actions.')
                end
                isDispatching = true
                local previousState = currentState
                currentState = currentReducer(currentState, action)
                isDispatching = false
                currentListeners = nextListeners
                local listeners = currentListeners
                for _, listener in ipairs(listeners) do
                    listener(previousState, currentState)
                end
                return action
            end

            local function replaceReducer(nextReducer)
                if(type(nextReducer) ~= 'function') then
                    error('Expected the nextReducer to be a function. Instead, received: '..type(nextReducer), 2)
                end

                currentReducer = nextReducer

                dispatch({ type = ActionTypes.REPLACE })
            end

            dispatch({ type = ActionTypes.INIT })

            store = {
                dispatch = dispatch,
                subscribe = subscribe,
                getState = getState,
                replaceReducer = replaceReducer,
            }
            redux.store = store
        end
    end

    redux = {
        createStore = createStore,
        combineReducers = combineReducers,
    }
end

return redux
