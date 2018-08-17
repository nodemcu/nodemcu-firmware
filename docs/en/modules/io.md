If you came here looking for the Lua `io` module that doesn't exist in NodeMCU, read on. 

The only mention of the `io` module in NodeMCU is in the [NodeMCU Developer FAQ](https://nodemcu.readthedocs.io/en/master/en/lua-developer-faq/), where it says 
"For example, the standard `io` and `os` libraries don't work, but have been largely replaced by the NodeMCU `node` and `file` libraries."

Unfortunately, if you are new to NodeMCU and Lua, there is little to go on to understand how to proceed. 

If you have read the Lua documentation on [The Simple I/O Model](https://www.lua.org/pil/21.1.html), 
you might be looking for io.write as a way to output text similar to print() but with more control.
With NodeMCU, the `file` library is suggested as a replacement, but it will not write to stdout like io.write. `file.write(string)` will not work because there is no file open.

It is not clear how to proceed without the `io` module. There is nothing analagous to stdout to open. 

The solution is to use the uart module. 

The Lua example `io.write("hello", "Lua")` would be written `uart.write(0,"hello", "Lua")`

Note that the UART ID value of 0 has to be added. UART 0 is the same one the the print() function uses.


