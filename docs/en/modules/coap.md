# CoAP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-02-04 | Toby Jaffey <toby@1248.io>, [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [coap.c](../../../app/modules/coap.c) |

The CoAP module provides a simple implementation according to [CoAP](http://tools.ietf.org/html/rfc7252) protocol.
The basic endpoint server part is based on [microcoap](https://github.com/1248/microcoap), and many other code reference [libcoap](https://github.com/obgm/libcoap).

This module implements both the client and the server side. GET/PUT/POST/DELETE is partially supported by the client. Server can register Lua functions and variables. No observe or discover supported yet.

!!! caution

    This module is only in the very early stages and not complete yet.


## Constants
Constants for various functions.

`coap.CON`, `coap.NON` represent the request types.

`coap.TEXT_PLAIN`, `coap.LINKFORMAT`, `coap.XML`, `coap.OCTET_STREAM`, `coap.EXI`, `coap.JSON` represent content types.

## coap.Client()

Creates a CoAP client.

#### Syntax
`coap.Client()`

#### Parameters
none

#### Returns
CoAP client

#### Example
```lua
cc = coap.Client()
-- assume there is a coap server at ip 192.168.100
cc:get(coap.CON, "coap://192.168.18.100:5683/.well-known/core")
-- GET is not complete, the result/payload only print out in console.
cc:post(coap.NON, "coap://192.168.18.100:5683/", "Hello")
```

## coap.Server()

Creates a CoAP server.

#### Syntax
`coap.Server()`

#### Parameters
none

#### Returns
CoAP server

#### Example
```lua
-- use copper addon for firefox
cs=coap.Server()
cs:listen(5683)

myvar=1
cs:var("myvar") -- get coap://192.168.18.103:5683/v1/v/myvar will return the value of myvar: 1

all='[1,2,3]'
cs:var("all", coap.JSON) -- sets content type to json

-- function should tack one string, return one string.
function myfun(payload)
  print("myfun called")
  respond = "hello"
  return respond
end
cs:func("myfun") -- post coap://192.168.18.103:5683/v1/f/myfun will call myfun

```

# CoAP Client

## coap.client:get()

Issues a GET request to the server.

#### Syntax
`coap.client:get(type, uri[, payload])`

#### Parameters
- `type` `coap.CON`, `coap.NON`, defaults to CON. If the type is CON and request fails, the library retries four more times before giving up.
- `uri` the URI such as "coap://192.168.18.103:5683/v1/v/myvar", only IP addresses are supported i.e. no hostname resoltion.
- `payload` optional, the payload will be put in the payload section of the request.

#### Returns
`nil`

## coap.client:put()

Issues a PUT request to the server.

#### Syntax
`coap.client:put(type, uri[, payload])`

#### Parameters
- `type` `coap.CON`, `coap.NON`, defaults to CON. If the type is CON and request fails, the library retries four more times before giving up.
- `uri` the URI such as "coap://192.168.18.103:5683/v1/v/myvar", only IP addresses are supported i.e. no hostname resoltion.
- `payload` optional, the payload will be put in the payload section of the request.

#### Returns
`nil`

## coap.client:post()

Issues a POST request to the server.

#### Syntax
`coap.client:post(type, uri[, payload])`

#### Parameters
- `type` coap.CON, coap.NON, defaults to CON. when type is CON, and request failed, the request will retry another 4 times before giving up.
- `uri` the uri such as coap://192.168.18.103:5683/v1/v/myvar, only IP is supported.
- `payload` optional, the payload will be put in the payload section of the request.

#### Returns
`nil`

## coap.client:delete()

Issues a DELETE request to the server.

#### Syntax
`coap.client:delete(type, uri[, payload])`

#### Parameters
- `type` `coap.CON`, `coap.NON`, defaults to CON. If the type is CON and request fails, the library retries four more times before giving up.
- `uri` the URI such as "coap://192.168.18.103:5683/v1/v/myvar", only IP addresses are supported i.e. no hostname resoltion.
- `payload` optional, the payload will be put in the payload section of the request.

#### Returns
`nil`

# CoAP Server

## coap.server:listen()

Starts the CoAP server on the given port.

#### Syntax
`coap.server:listen(port[, ip])`

#### Parameters
- `port` server port (number)
- `ip` optional IP address

#### Returns
`nil`

## coap.server:close()

Closes the CoAP server.

#### Syntax
`coap.server:close()`

#### Parameters
none

#### Returns
`nil`

## coap.server:var()

Registers a Lua variable as an endpoint in the server. the variable value then can be retrieved by a client via GET method, represented as an [URI](http://tools.ietf.org/html/rfc7252#section-6) to the client. The endpoint path for varialble is '/v1/v/'.

#### Syntax
`coap.server:var(name[, content_type])`

#### Parameters
- `name` the Lua variable's name
- `content_type` optional, defaults to `coap.TEXT_PLAIN`, see [Content Negotiation](http://tools.ietf.org/html/rfc7252#section-5.5.4)

#### Returns
`nil`

#### Example
```lua
-- use copper addon for firefox
cs=coap.Server()
cs:listen(5683)

myvar=1
cs:var("myvar") -- get coap://192.168.18.103:5683/v1/v/myvar will return the value of myvar: 1
-- cs:var(myvar), WRONG, this api accept the name string of the varialbe. but not the variable itself.
all='[1,2,3]'
cs:var("all", coap.JSON) -- sets content type to json
```

## coap.server:func()

Registers a Lua function as an endpoint in the server. The function then can be called by a client via POST method. represented as an [URI](http://tools.ietf.org/html/rfc7252#section-6) to the client. The endpoint path for function is '/v1/f/'. 

When the client issues a POST request to this URI, the payload will be passed to the function as parameter. The function's return value will be the payload in the message to the client.

The function registered SHOULD accept ONLY ONE string type parameter, and return ONE string value or return nothing.

#### Syntax
`coap.server:func(name[, content_type])`

#### Parameters
- `name` the Lua function's name
- `content_type` optional, defaults to `coap.TEXT_PLAIN`, see [Content Negotiation](http://tools.ietf.org/html/rfc7252#section-5.5.4)

#### Returns
`nil`

#### Example
```lua
-- use copper addon for firefox
cs=coap.Server()
cs:listen(5683)

-- function should take only one string, return one string.
function myfun(payload)
  print("myfun called")
  respond = "hello"
  return respond
end
cs:func("myfun") -- post coap://192.168.18.103:5683/v1/f/myfun will call myfun
-- cs:func(myfun), WRONG, this api accept the name string of the function. but not the function itself.
```
