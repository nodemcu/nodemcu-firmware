# sqlite3 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-06-20 | [Luiz Felipe Silva](https://github.com/luizfeliperj) | [Luiz Felipe Silva](https://github.com/luizfeliperj) | [sqlite3.c](../../app/modules/sqlite3.c)|

!!! attention
    ###This module is currently not available.

    Even if you enable it in configuration it will not be available.
        
    In brief this is due to a lack of proof of usability. The memory constraints on the ESP8266 are just too tight.

    The module is not removed yet as it might be revived to run on the ESP32 after the two branches for ESP8266 and ESP32 have been unified.

    Please see [#2913](https://github.com/nodemcu/nodemcu-firmware/issues/2913) for more information.

This module is based on [LuaSQLite3](http://lua.sqlite.org/index.cgi/index) module developed by Tiago Dionizio and Doug Currie with contributions from Thomas Lauer, Michael Roth, and Wolfgang Oertl.

This module depens on [SQLite3](http://www.sqlite.org/) library developed by Dwayne Richard Hipp.

For instruction on how to use this module or further documentation, please, refer to [LuaSQLite3 Documentation](http://lua.sqlite.org/index.cgi/doc/tip/doc/lsqlite3.wiki).

This module is a stripped down version of SQLite, with every possible OMIT_\* configuration enable. The enabled OMIT_\* directives are available in the module's [config file](../../app/sqlite3/config_ext.h).

The SQLite3 module vfs layer integration with NodeMCU was developed by me.

**Simple example**

```lua
db = sqlite3.open_memory()

db:exec[[
  CREATE TABLE test (id INTEGER PRIMARY KEY, content);

  INSERT INTO test VALUES (NULL, 'Hello, World');
  INSERT INTO test VALUES (NULL, 'Hello, Lua');
  INSERT INTO test VALUES (NULL, 'Hello, Sqlite3')
]]

for row in db:nrows("SELECT * FROM test") do
  print(row.id, row.content)
end
```
