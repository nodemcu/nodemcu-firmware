# sqlite3 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-06-20 | [Luiz Felipe Silva](https://github.com/luizfeliperj) | [Luiz Felipe Silva](https://github.com/luizfeliperj) | [sqlite3.c](../../../app/modules/sqlite3.c)|

This module is based on [LuaSQLite3](http://lua.sqlite.org/index.cgi/index) module developed by Tiago Dionizio and Doug Currie with contributions from Thomas Lauer, Michael Roth, and Wolfgang Oertl.

This module depens on [SQLite3](http://www.sqlite.org/) library developed by Dwayne Richard Hipp.

For instruction on how to use this module or further documentation, please, refer to [LuaSQLite3 Documentation](http://lua.sqlite.org/index.cgi/doc/tip/doc/lsqlite3.wiki).

This module is a stripped down version of SQLite, with every possible OMIT_\* configuration enable. The enabled OMIT_\* directives are available in the module's [config file](../../../app/sqlite3/config_ext.h).

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