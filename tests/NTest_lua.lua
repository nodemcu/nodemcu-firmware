local N = require "NTest" ("Lua detail tests")

N.test('typeerror', function()
  fail(function() math.abs("") end, "number expected, got string", "string")
  fail(function() math.abs() end, "number expected, got no value", "no value")
end)
