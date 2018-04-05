# FTPServer Module

This is a Lua module to access the SPIFFS file system via FTP protocol. 
FTP server uses only one specified user and password to authenticate clients.
All clients need authentication - anonymous access is not supported yet.
All files have RW access.
Directory creation do not supported because SPIFFS do not have that.

## Require
```lua
ftpserver = require("ftpserver")
```
## Release
```lua
ftpserver:closeServer()
ftpserver = nil
package.loaded["ftpserver"]=nil
```

# Methods

## createServer()
Starts listen on 20 and 21 ports for serve FTP clients. 

The module requires active network connection.

#### Syntax
`createServer(username, password)`

#### Parameters
- `username` string username for 'USER username' ftp command.
- `password` string password.

#### Returns
nil

#### Example
```lua
require("ftpserver").createServer("test","12345")
```

## closeServer()
Closes all server sockets.

#### Syntax
`closeServer()`

#### Returns
nil

#### Example
```lua
ftpserver = require("ftpserver")
ftpserver:createServer("test","12345")
-------------------------
-- Some program code
-------------------------
if needStopFtp = true then
  ftpserver:closeServer()
  ftpserver = nil
  package.loaded["ftpserver"] = nil
end
```

