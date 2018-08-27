# Telnet Module

| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) |  [Terry Ellison](https://github.com/TerryE) | [simple_telnet.lua](./simple_telnet.lua) |
| 2018-05-24 | [Terry Ellison](https://github.com/TerryE) |  [Terry Ellison](https://github.com/TerryE) | [telnet.lua](./telnet.lua) |


The Lua telnet example previously provided in our distro has been moved to this 
file `simple_telnet.lua` in this folder. This README discusses the version complex 
implementation at the Lua module `telnet.lua`.  The main reason for this complex
alternative is that a single Lua command can produce a LOT of output, and the 
telnet server has to work within four constraints:

-  The SDK rules are that you can only issue one send per task invocation, so any 
overflow must be buffered, and the buffer emptied using an on:sent callback (CB).

-  Since the interpeter invokes a node.output CB per field, you have a double whammy 
that these fields are typically small, so using a simple array FIFO would rapidly 
exhaust RAM.

-  For network efficiency, the module aggregates any FIFO buffered into sensible
sized packet, say 1024 bytes, but it must also need to handle the case when larger
string span multiple packets. However, you must flush the buffer if necessary.

-  The overall buffering strategy needs to be reasonably memory efficient and avoid
hitting the GC too hard, so where practical avoid aggregating small strings to more 
than 256 chars (as NodeMCU handles \<256 using stack buffers), and avoid serial
aggregation such as buf = buf .. str as this hammers the GC.
 
So this server adopts a simple buffering scheme using a two level FIFO. The 
`node.output` CB adds records to the 1st level FIFO until the #recs is \> 32 or the 
total size would exceed 256 bytes. Once over this threashold, the contents of the 
FIFO are concatenated into a 2nd level FIFO entry of upto 256 bytes, and the 1st 
level FIFO cleared down to any residue.

The sender dumps the 2nd level FIFO aggregating records up to 1024 bytes and once this
is empty dumps an aggrate of the 1st level.

Lastly remember that owing to architectural limitations of the firmware, this server
can only service stdin and stdout.  Lua errors are still sent to stderr which is
the UART0 device.  Hence errors will fail silently.  If you want to capture 
errors then you will need to wrap any commands in a `pcall()` and print any
error return.

## telnet:open()

Open a telnet server based on the provided parameters.

#### Syntax

`telnet:open(ssid, pwd, port)`

#### Parameters

`ssid` and `password`.  Strings.  SSID and Password for the Wifi network.  If these are 
`nil` then the wifi is assumed to be configured or autoconfigured.

`port`.  Integer TCP listenting port for the Telnet service.  The default is 2323
 
#### Returns

Nothing returned (this is evaluted as `nil` in a scalar context).

## telnet:close()

Close a telnet server and release all resources.

#### Syntax

`telnet:close()`

#### Parameters

None

#### Returns

Nothing returned (this is evaluted as `nil` in a scalar context).
