local FILE_READ_CHUNK = 1024

local _file = file
local file_exists, file_open, file_getcontents, file_rename, file_stat, file_putcontents, file_close, file_list =
  _file.exists, _file.open, _file.getcontents, _file.rename, _file.stat, _file.putcontents, _file.close, _file.list
local node_LFS_resource = node.LFS.resource or function(filename) if filename then return else return {} end end -- luacheck: ignore

local file_lfs = {}
local current_file_lfs

local function file_lfs_create (filename)
  local content = node_LFS_resource(filename)
  local pos = 1

  local read = function (_, n_or_char)
    local p1, p2
    n_or_char = n_or_char or FILE_READ_CHUNK
    if type(n_or_char) == "number" then
      p1, p2 = pos, pos + n_or_char - 1
    elseif type(n_or_char) == "string" and #n_or_char == 1 then
      p1 = pos
      local _
      _, p2 = content:find(n_or_char, p1, true)
      if not p2 then p2 = p1 + FILE_READ_CHUNK - 1 end
    else
      error("invalid parameter")
    end
    if p2 - p1 > FILE_READ_CHUNK then p2 = pos + FILE_READ_CHUNK - 1 end
    if p1>#content then return end
    pos = p2+1
    return content:sub(p1, p2)
  end

  local seek = function (_, whence, offset)
    offset = offset or 0
    local len = #content + 1 -- position starts at 1
    if whence == "set" then
      pos = offset + 1 -- 0 offset means position 1
    elseif whence == "cur" then
      pos = pos + offset
    elseif whence == "end" then
      pos = len + offset
    elseif whence then -- not nil not one of above
      return -- on error return nil
    end
    local pos_ok = true
    if pos < 1 then pos = 1; pos_ok = false end
    if pos > len then pos = len; pos_ok = false end
    return pos_ok and pos - 1 or nil
  end

  local obj = {}
  obj.read = read
  obj.readline = function(self) return read(self, "\n") end
  obj.seek = seek
  obj.close = function() current_file_lfs = nil end

  setmetatable(obj, {
    __index = function (_, k)
      return function () --...)
        error (("LFS file unsupported function '%s'") % tostring(k))
        --return _file[k](...)
      end
    end
  })

  return obj
end

file_lfs.exists = function (filename)
  return (node_LFS_resource(filename) ~= nil) or file_exists(filename)
end

file_lfs.open = function (filename, mode)
  mode = mode or "r"
  if file_exists(filename) then
    return file_open(filename, mode)
  elseif node_LFS_resource(filename) then
    if mode ~=  "r" and mode:find("^[rwa]%+?$") then
      -- file does not exist in SPIFFS but exists in LFS -> copy to SPIFFS
      file_putcontents(filename, node_LFS_resource(filename))
      return file_open(filename, mode)
    else -- "r" or anything else
      current_file_lfs = file_lfs_create (filename)
      return current_file_lfs
    end
  else
    return file_open(filename, mode)
  end
end

file_lfs.close = function (...)
  current_file_lfs = nil
  return file_close(...)
end

file_lfs.getcontents = function(filename)
  if file_exists(filename) then
    return file_getcontents(filename)
  else
    return node_LFS_resource(filename) or file_getcontents(filename)
  end
end

file_lfs.rename = function(oldname, newname)
  if node_LFS_resource(oldname) ~= nil and not file_exists(oldname) then
    -- error "LFS file cannot be renamed"
    return false
  else
    return file_rename(oldname, newname)
  end
end

file_lfs.stat = function(filename)
  if node_LFS_resource(filename) ~= nil and not file_exists(filename) then
    return {
      name = filename,
      size = #node_LFS_resource(filename),
      time = {day = 1, hour = 0, min = 0, year = 1970, sec = 0, mon = 1},
      is_hidden = false, is_rdonly = true, is_dir = false, is_arch = false, is_sys = false, is_LFS = true
    }
  else
    return file_stat(filename)
  end
end

file_lfs.list = function (pattern, SPIFFs_only)
  local filelist = file_list(pattern)
  if not SPIFFs_only then
    local fl = node_LFS_resource()
    if fl then
      for _, f in ipairs(fl) do
        if not(filelist[f]) and (not pattern or f:match(pattern)) then
          filelist[f] = #node_LFS_resource(f)
        end
      end
    end
  end
  return filelist
end

setmetatable(file_lfs, {
  __index = function (_, k)
    return function (...)
        local t = ...
        if type(t) == "table" then
          return t[k](...)
        elseif not t and current_file_lfs then
          return current_file_lfs[k](...)
        else
          return _file[k](...)
        end
      end
  end
})

print ("[file_lfs] LFS file routines loaded")
return file_lfs
