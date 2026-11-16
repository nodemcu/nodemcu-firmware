-- A series of convenient utility functions for use with NTest on NodeMCU
-- devices under test (DUTs).

local NTE = {}

-- Determine if the firmware has been built with a given module
function NTE.hasC(m)
  -- this isn't great, but it's what we've got
  local mstr = node.info('build_config').modules
  return mstr:match("^"..m..",") -- first module in the list
      or mstr:match(","..m.."$") -- last module in the list
      or mstr:match(","..m..",") -- somewhere else
end

-- Determine if a given Lua module is available for require-ing.
function NTE.hasL(m)
  for _, l in ipairs(package.loaders) do
    if type(l(m)) == "function" then return true end
  end
  return false
end

-- Look up one (or more) feature keys in our configuration file,
-- returning the associated value (or a map from keys to values).
function NTE.getFeat(...)
  -- Stream through our configuration file and extract the features attested
  --
  -- We expect the configuration file to attest features as keys in a dictionary
  -- so that they can be efficiently probed here but also so that we can
  -- parameterize features.
  --
  -- {
  --   "feat1" : ... ,
  --   "feat2" : ...
  -- }
  local reqFeats = { ... }
  local decoder = sjson.decoder({
    metatable = {
      checkpath = function(_, p)
        if #p > 1 then return true end            -- something we're keeping
        if #p == 0 then return true end           -- root table
        local thisFeat = p[1]
        assert (type(thisFeat) == "string")
        for _, v in ipairs(reqFeats) do
          if v == thisFeat then return true end   -- requested feature
        end
        return false
      end
    }
  })
  local cfgf = file.open("testenv.conf", "r")

  if cfgf == nil then return {} end -- no features if no config file

  local cstr
  repeat
    cstr = cfgf:read(); decoder:write(cstr or "")
  until (not cstr or #cstr == 0)
  cfgf:close()
  local givenFeats = decoder:result()
  assert (type(givenFeats) == "table", "Malformed configuration file")

  local res = {}
  for _, v in ipairs(reqFeats) do
    res[v] = givenFeats[v] or error("Missing required feature " .. v)
  end

  if #reqFeats == 1 then return res[reqFeats[1]] end
  return res
end

return NTE
