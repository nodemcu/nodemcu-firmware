# CoAP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-02-04 | Toby Jaffey <toby@1248.io>, [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [coap.c](../../../app/modules/coap.c) |

The CoAP module provides a simple implementation according to [CoAP](http://tools.ietf.org/html/rfc7252) protocol.
The basic endpoint server part is based on [microcoap](https://github.com/1248/microcoap), and many other code reference [libcoap](https://github.com/obgm/libcoap).

This module implements both the client and the server side.It can support basic usage No observe or discover supported yet.

!!! caution

    This module is only in the early stages and not complete yet.


## Constants
Constants for various functions.

### Request Types

`coap.type.con`, `coap.type.non` represent the request types.

### Methods

`coap.method.get`, `coap.method.post`, `coap.method.put`, `coap.method.delete` represent Methods.

### Content Types

`coap.content_type.none`, `coap.content_type.text_plain`, `coap.content_type.link_format`, `coap.content_type.xml`, `coap.content_type.exi`, `coap.content_type.json` represent content types.

### Codes

`coap.code.created`, `coap.code.deleted`, `coap.code.vaild`, `coap.code.changed`, `coap.code.content`, `coap.code.bad_request`, `coap.code.unauthorized`, `coap.code.bad_option`, `coap.code.forbidden`, `coap.code.not_found`, `coap.code.method_not_allowed`, `coap.code.precondition_failed`, `coap.code.request_entity_too_large`, `coap.code.unsupported_content_format`, `coap.code.internal_server_error`, `coap.code.not_implemented`, `coap.code.bad_gateway`, `coap.code.service_unavailable`, `coap.code.gateway_timeout`, `coap.code.method_not_allowed`, `coap.code.proxying_not_supported` represent Codes.


## coap.new()

Creates a CoAP instance.

#### Syntax
`coap.new()`

#### Parameters
none

#### Returns
CoAP peer instance

#### Example
```lua
-- Create UDP Socket to send or receive data
u = net.createUDPSocket()
-- Create CoAP instance to handle data;
c = coap.new()
u:listen(5683)
-- The function is called when data is received
u:on("receive", function(s, data, port, ip) c:receive(data,ip,port) end)
-- Set CoAP sender
c:sender(function(data,ip,port) u:send(port,ip,data) end)

```


## coap.peer:handler()

Set the CoAP server request handler.

#### Syntax
`coap.peer:handler(function(pkt,ip,port))`

#### Parameters
- `function(data,ip,port)`callback function.

The first parameter of callback is the `CoAP package` instance that receive from remote.
The second and third parameter of callback is the address of the data being received.

You must return the first argument after processing the request.

#### Returns
`nil`

## coap.peer:receive()

This function must be called when you receive data from UDP Socket.

#### Syntax
`coap.peer:receive(data, ip, port)`

#### Parameters
- `data`, the data received from UDP Socket.
- `ip`, the source IP of UDP data.
- `port` the source port of UDP data.

#### Returns
`nil`


## coap.peer:request()

Issues a CoAP request to the server.

#### Syntax
`coap.peer:request(method [,type], uri[, payload, handler])`

#### Parameters
- `method` `coap.method.get`, `coap.method.post`, `coap.method.put`, `coap.method.delete`. You can specify a CoAP request method
- `type` `coap.CON`, `coap.NON`, defaults to NON. If the type is CON and request fails, the library retries four more times before giving up.
- `uri` the URI such as "coap://192.168.18.103:5683/path/to/resource", only IP addresses are supported. If you want to use the host name, please resolve it in the sender function.
- `payload` optional, the payload will be put in the payload section of the request.
- `handler` optional, the callback function is called when a CON reply is received. Only supports CON messages.

#### Returns
`nil`

## coap.peer:sender()

Set the sender function for the CoAP instance.

#### Syntax
`coap.peer:sender(function(data,ip,port))`

#### Parameters

- `function(data,ip,port)`callback function.

The first parameter of callback is the data that needs to be sent. The second and third parameter of callback is the address of the data being sent.

#### Returns
`nil`

## CoAP package

Received or issued CoAP packet.

The CoAP package is a Userdata similar to a table. You can read and write the specified index on the object

#### Indexs
- `code` ,the methods and codes
- `type` ,read only. mid is message id
- `token` ,CoAP token
- `options` ,CoAP options.The headers of the request or response are recorded. You can read or write a lua table
- `payload` ,Request's or response's payload.

#### Example
```
c:handler(function(pkt,ip,port)
    print("request method is",pkt.code)
    pkt.code = coap.code.content
    pkt.options = {ooap.options.content_format, coap.content_type.text_plain}
    pkt.payload = "hello"
    return pkt
end) 
```
