-- Generic utility functions

module( ..., package.seeall )

local lfs = require "lfs"
local sf = string.format
-- Taken from Lake
dir_sep = package.config:sub( 1, 1 )
is_os_windows = dir_sep == '\\'

-- Converts a string with items separated by 'sep' into a table
string_to_table = function( s, sep )
  if type( s ) ~= "string" then return end
  sep = sep or ' '
  if s:sub( -1, -1 ) ~= sep then s = s .. sep end
  s = s:gsub( sf( "^%s*", sep ), "" )
  local t = {}
  local fmt = sf( "(.-)%s+", sep )
  for w in s:gmatch( fmt ) do table.insert( t, w ) end
  return t
end

-- Split a file name into 'path part' and 'extension part'
split_ext = function( s )
  local pos
  for i = #s, 1, -1 do
    if s:sub( i, i ) == "." then
      pos = i
      break
    end
  end
  if not pos or s:find( dir_sep, pos + 1 ) then return s end
  return s:sub( 1, pos - 1 ), s:sub( pos )
end

-- Replace the extension of a given file name
replace_extension = function( s, newext )
  local p, e = split_ext( s )
  if e then 
    if newext and #newext > 0 then 
      s = p .. "." .. newext
    else
      s = p
    end
  end
  return s
end

-- Return 'true' if building from Windows, false otherwise
is_windows = function()
  return is_os_windows
end

-- Prepend each component of a 'pat'-separated string with 'prefix'
prepend_string = function( s, prefix, pat )  
  if not s or #s == 0 then return "" end
  pat = pat or ' '
  local res = ''
  local st = string_to_table( s, pat )
  foreach( st, function( k, v ) res = res .. prefix .. v .. " " end )
  return res
end

-- Like above, but consider 'prefix' a path
prepend_path = function( s, prefix, pat )
  return prepend_string( s, prefix .. dir_sep, pat )
end

-- full mkdir: create all the paths needed for a multipath
full_mkdir = function( path )
  local ptables = string_to_table( path, dir_sep )
  local p, res = ''
  for i = 1, #ptables do
    p = ( i ~= 1 and p .. dir_sep or p ) .. ptables[ i ]
    res = lfs.mkdir( p )
  end
  return res
end

-- Concatenate the given paths to form a complete path
concat_path = function( paths )
  return table.concat( paths, dir_sep )
end

-- Return true if the given array contains the given element, false otherwise
array_element_index = function( arr, element )
  for i = 1, #arr do
    if arr[ i ] == element then return i end
  end
end

-- Linearize an array with (possibly) embedded arrays into a simple array
_linearize_array = function( arr, res, filter )
  if type( arr ) ~= "table" then return end
  for i = 1, #arr do
    local e = arr[ i ]
    if type( e ) == 'table' and filter( e ) then
      _linearize_array( e, res, filter )
    else
      table.insert( res, e )
    end
  end 
end

linearize_array = function( arr, filter )
  local res = {}
  filter = filter or function( v ) return true end
  _linearize_array( arr, res, filter )
  return res
end

-- Return an array with the keys of a table
table_keys = function( t )
  local keys = {}
  foreach( t, function( k, v ) table.insert( keys, k ) end )
  return keys
end

-- Return an array with the values of a table
table_values = function( t )
  local vals = {}
  foreach( t, function( k, v ) table.insert( vals, v ) end )
  return vals
end

-- Returns true if 'path' is a regular file, false otherwise
is_file = function( path )
  return lfs.attributes( path, "mode" ) == "file"
end

-- Returns true if 'path' is a directory, false otherwise
is_dir = function( path )
  return lfs.attributes( path, "mode" ) == "directory"
end

-- Return a list of files in the given directory matching a given mask
get_files = function( path, mask, norec, level )
  local t = ''
  level = level or 0
  for f in lfs.dir( path ) do
    local fname = path .. dir_sep .. f
    if lfs.attributes( fname, "mode" ) == "file" then
      local include
      if type( mask ) == "string" then
        include = fname:find( mask )
      else
        include = mask( fname )
      end
      if include then t = t .. ' ' .. fname end
    elseif lfs.attributes( fname, "mode" ) == "directory" and not fname:find( "%.+$" ) and not norec then
      t = t .. " " .. get_files( fname, mask, norec, level + 1 )
    end
  end
  return level > 0 and t or t:gsub( "^%s+", "" )
end

-- Check if the given command can be executed properly
check_command = function( cmd )
  local res = os.execute( cmd .. " > .build.temp 2>&1" )
  os.remove( ".build.temp" )
  return res
end

-- Execute a command and capture output
-- From: http://stackoverflow.com/a/326715/105950
exec_capture = function( cmd, raw )
  local f = assert(io.popen(cmd, 'r'))
  local s = assert(f:read('*a'))
   f:close()
   if raw then return s end
   s = string.gsub(s, '^%s+', '')
   s = string.gsub(s, '%s+$', '')
   s = string.gsub(s, '[\n\r]+', ' ')
   return s
end

-- Execute the given command for each value in a table
foreach = function ( t, cmd )
  if type( t ) ~= "table" then return end
  for k, v in pairs( t ) do cmd( k, v ) end
end

-- Generate header with the given #defines, return result as string
gen_header_string = function( name, defines )
  local s = "// eLua " .. name:lower() .. " definition\n\n"
  s = s .. "#ifndef __" .. name:upper() .. "_H__\n"
  s = s .. "#define __" .. name:upper() .. "_H__\n\n"

  for key,value in pairs(defines) do 
     s = s .. string.format("#define   %-25s%-19s\n",key:upper(),value)
  end

  s = s .. "\n#endif\n"
  return s
end

-- Generate header with the given #defines, save result to file
gen_header_file = function( name, defines )
  local hname = concat_path{ "inc", name:lower() .. ".h" }
  local h = assert( io.open( hname, "w" ) )
  h:write( gen_header_string( name, defines ) )
  h:close()
end

-- Remove the given elements from an array
remove_array_elements = function( arr, del )
  del = istable( del ) and del or { del }
  foreach( del, function( k, v )
    local pos = array_element_index( arr, v )
    if pos then table.remove( arr, pos ) end
  end )
end

-- Remove a directory recusively
-- USE WITH CARE!! Doesn't do much checks :)
rmdir_rec = function ( dirname )
  if lfs.attributes( dirname, "mode" ) ~= "directory" then return end
  for f in lfs.dir( dirname ) do
    local ename = string.format( "%s/%s", dirname, f )
    local attrs = lfs.attributes( ename )
    if attrs.mode == 'directory' and f ~= '.' and f ~= '..' then
      rmdir_rec( ename ) 
    elseif attrs.mode == 'file' or attrs.mode == 'named pipe' or attrs.mode == 'link' then
      os.remove( ename )
    end
  end
  lfs.rmdir( dirname )
end

-- Concatenates the second table into the first one
concat_tables = function( dst, src )
  foreach( src, function( k, v ) dst[ k ] = v end )
end

-------------------------------------------------------------------------------
-- Color-related funtions
-- Currently disabled when running in Windows
-- (they can be enabled by setting WIN_ANSI_TERM)

local dcoltable = { 'black', 'red', 'green', 'yellow', 'blue', 'magenta', 'cyan', 'white' }
local coltable = {}
foreach( dcoltable, function( k, v ) coltable[ v ] = k - 1 end )

local _col_builder = function( col )
  local _col_maker = function( s )
    if is_os_windows and not os.getenv( "WIN_ANSI_TERM" ) then
      return s
    else
      return( sf( "\027[%d;1m%s\027[m", coltable[ col ] + 30, s ) )
    end
  end
  return _col_maker
end

col_funcs = {}
foreach( coltable, function( k, v ) 
  local fname = "col_" .. k
  _G[ fname ] = _col_builder( k ) 
  col_funcs[ k ] = _G[ fname ]
end )

-------------------------------------------------------------------------------
-- Option handling

local options = {}

options.new = function()
  local self = {}
  self.options = {}
  setmetatable( self, { __index = options } )
  return self
end

-- Argument validator: boolean value
options._bool_validator = function( v )
  if v == '0' or v:upper() == 'FALSE' then
    return false
  elseif v == '1' or v:upper() == 'TRUE' then
    return true
  end
end

-- Argument validator: choice value
options._choice_validator = function( v, allowed )
  for i = 1, #allowed do
    if v:upper() == allowed[ i ]:upper() then return allowed[ i ] end
  end
end

-- Argument validator: choice map (argument value maps to something)
options._choice_map_validator = function( v, allowed )
  for k, value in pairs( allowed ) do
    if v:upper() == k:upper() then return value end
  end
end

-- Argument validator: string value (no validation)
options._string_validator = function( v )
  return v
end

-- Argument printer: boolean value
options._bool_printer = function( o )
  return "true|false", o.default and "true" or "false"
end

-- Argument printer: choice value
options._choice_printer = function( o )
  local clist, opts  = '', o.data
  for i = 1, #opts do
    clist = clist .. ( i ~= 1 and "|" or "" ) .. opts[ i ]
  end
  return clist, o.default
end

-- Argument printer: choice map printer
options._choice_map_printer = function( o )
  local clist, opts, def = '', o.data
  local i = 1
  for k, v in pairs( opts ) do
    clist = clist .. ( i ~= 1 and "|" or "" ) .. k
    if o.default == v then def = k end
    i = i + 1
  end
  return clist, def
end

-- Argument printer: string printer
options._string_printer = function( o )
  return nil, o.default
end

-- Add an option of the specified type
options._add_option = function( self, optname, opttype, help, default, data )
  local validators = 
  { 
    string = options._string_validator, choice = options._choice_validator, 
    boolean = options._bool_validator, choice_map = options._choice_map_validator
  }
  local printers = 
  { 
    string = options._string_printer, choice = options._choice_printer, 
    boolean = options._bool_printer, choice_map = options._choice_map_printer
  }
  if not validators[ opttype ] then
    print( sf( "[builder] Invalid option type '%s'", opttype ) )
    os.exit( 1 )
  end
  table.insert( self.options, { name = optname, help = help, validator = validators[ opttype ], printer = printers[ opttype ], data = data, default = default } )
end

-- Find an option with the given name
options._find_option = function( self, optname )
  for i = 1, #self.options do
    local o = self.options[ i ]
    if o.name:upper() == optname:upper() then return self.options[ i ] end
  end
end

-- 'add option' helper (automatically detects option type)
options.add_option = function( self, name, help, default, data )
  local otype
  if type( default ) == 'boolean' then
    otype = 'boolean'
  elseif data and type( data ) == 'table' and #data == 0 then
    otype = 'choice_map'
  elseif data and type( data ) == 'table' then
    otype = 'choice'
    data = linearize_array( data )
  elseif type( default ) == 'string' then
    otype = 'string'
  else
    print( sf( "Error: cannot detect option type for '%s'", name ) )
    os.exit( 1 )
  end
  self:_add_option( name, otype, help, default, data )
end

options.get_num_opts = function( self )
  return #self.options
end

options.get_option = function( self, i )
  return self.options[ i ]
end

-- Handle an option of type 'key=value'
-- Returns both the key and the value or nil for error
options.handle_arg = function( self, a )
  local si, ei, k, v = a:find( "([^=]+)=(.*)$" )
  if not k or not v then 
    print( sf( "Error: invalid syntax in '%s'", a ) )
    return
  end
  local opt = self:_find_option( k )
  if not opt then
    print( sf( "Error: invalid option '%s'", k ) )
    return
  end
  local optv = opt.validator( v, opt.data )
  if optv == nil then
    print( sf( "Error: invalid value '%s' for option '%s'", v, k ) )
    return
  end
  return k, optv
end

-- Show help for all the registered options
options.show_help = function( self )
  for i = 1, #self.options do
    local o = self.options[ i ]
    print( sf( "\n  %s: %s", o.name, o.help ) )
    local values, default = o.printer( o )
    if values then
      print( sf( "    Possible values: %s", values ) )
    end
    print( sf( "    Default value: %s", default or "none (changes at runtime)" ) )
  end
end

-- Create a new option handler
function options_handler()
  return options.new()
end

