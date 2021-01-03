stds.nodemcu_libs = {}
loadfile ("tools/luacheck_config.lua")(stds)
local empty = { }

stds.nodemcu_libs.read_globals.ok = empty
stds.nodemcu_libs.read_globals.nok = empty
stds.nodemcu_libs.read_globals.eq = empty
stds.nodemcu_libs.read_globals.fail = empty
stds.nodemcu_libs.read_globals.spy = empty

std = "lua51+lua53+nodemcu_libs"
