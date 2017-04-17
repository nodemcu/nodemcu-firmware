#! /usr/bin/lua
--[[
This Lua script will build your user_config.h and user_modules.h file in your app/includes
directory.  Maintain a user.cnf file in this directory and then run 

  tools/generate_user_modules.lua 

from the root nodemcy directory.  The cnf file contains directives of the following form,
and most have sensible defaults so only need to specify the ones that you want to override.

;
; this is a comment and ignored
;
flash_size=1M
bit_rate         = 115200
client_ssl       = disable
sha2             = disable
modules = node,file,gpio,wifi,net,tmr,uart,ow,bit

You can optionally path in an explicit config file if you have variants.
]]
if not pcall(require,"lfs") then 
  print "This module needs the lua filesystem installed" os.exit(1) 
end

local FALSE = {}
local USER_OPTS = {
  BUILTIN = {"string","table","coroutine","math","debug_minimal"},
  SET = {                               -- Define if true or omit
    FLASH_SAFE_API = true,
    ESP_INIT_DATA_ENABLE_READVDD33 = true,
    ESP_INIT_DATA_ENABLE_READADC = FALSE,
    NODE_ERROR = true,
    NODE_DBG = FALSE,
    DEVKIT_VERSION_0_9 = FALSE,
    LUA_NUMBER_INTEGRAL = FALSE,
  },
  ENABLE_OMIT = {                       -- Define with _ENABLED suffix or omit
    CLIENT_SSL = 1,
    GPIO_INTERRUPT = 1,
    MD2 = 0,
    SHA2 = 1,
  },
  SET_OMIT = {                          -- Define as value or omit if false
    BUILD_WOFS = FALSE,
    BUILD_SPIFFS = 1,
    SPIFFS_CACHE = 1,
    NODE_DEBUG = FALSE,
  },
  VALUES = {                            -- Define as value taking default
    FLASH_SIZE = false,                    -- This one has to be false not FALSE
    ESP_INIT_DATA_FIXED_VDD33_VALUE = 33,
    BIT_RATE_DEFAULT = 115200,
    READLINE_INTERVAL = 80,
    LUA_OPTIMIZE_DEBUG = 2,
    STRBUF_DEFAULT_INCREMENT = 32,
    ENDUSER_SETUP_AP_SSID = '"SetupGadget"',
  },
}

local FLASH_SIZE = {"512K", "1M", "2M", "4M", "8M", "16M"}
local USER_CONFIG = [[
#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

%s
#define FLASH_AUTOSIZE

#ifdef NODE_DEBUG
#define NODE_DBG c_printf
#else
#define NODE_DBG
#endif	/* NODE_DEBUG */

#ifdef NODE_ERROR
#define NODE_ERR c_printf
#else
#define NODE_ERR
#endif  /* NODE_ERROR */

#define LUA_TASK_PRIO         USER_TASK_PRIO_0
#define LUA_PROCESS_LINE_SIG  2

#define ICACHE_STORE_TYPEDEF_ATTR __attribute__((aligned(4),packed))
#define ICACHE_STORE_ATTR __attribute__((aligned(4)))
#define ICACHE_RAM_ATTR __attribute__((section(".iram0.text")))

#define LUA_OPTRAM
#define LUA_OPTIMIZE_MEMORY 2

#ifdef DEVKIT_VERSION_0_9
#  define KEYLED_INTERVAL  80
#  define KEY_SHORT_MS  200
#  define KEY_LONG_MS    3000
#  define KEY_SHORT_COUNT (KEY_SHORT_MS / READLINE_INTERVAL)
#  define KEY_LONG_COUNT (KEY_LONG_MS / READLINE_INTERVAL)
#  define LED_HIGH_COUNT_DEFAULT 10
#  define LED_LOW_COUNT_DEFAULT 0
#endif

#endif  /* __USER_CONFIG_H__ */
]]

local USER_MODULES = [[
#ifndef __USER_MODULES_H__
#define __USER_MODULES_H__

%s

#endif /* LUA_CROSS_COMPILER */
#endif  /* __USER_MODULES_H__ */
]]

local MODULE_PREFIX  = '#define LUA_USE_MODULES_'
local CORE_PREFIX    = '#define LUA_USE_BUILTIN_'
local option = {}

local function parseConfig(filename)
  local key = {}
  for c,t in pairs(USER_OPTS) do
    if c ~= "BUILTIN" then for k in pairs(t) do key[k:lower()] = c end end
  end

  assert( filename:sub(-4) == ".cfg" )
  local cfg = assert( io.input(filename) )

  for line in cfg:lines() do

    local s,e, verb, params = line:find('^%s*([%w_]+)%s*=%s*("?.*)')
    
    if verb then
      verb = verb:lower()
      if verb == "builtin" or verb == "modules" then
        local value, val_list = {}, assert( "," .. params )
        for v in val_list:gmatch( '%s*,%s*([%w_]+)' ) do
          value[#value+1] = v:lower()
        end
        assert( #value )
        USER_OPTS[verb:upper()] = value
      elseif key[verb] then
        local s,e = params:find('%s*$')
        if s then params = params:sub(1,s-1) end
        if params:lower() == "enable" then 
           params = true 
        elseif params:lower() == "disable" then 
           params = false
        elseif params:lower() == "true" then 
           params = true
        elseif params:lower() == "false" then 
           params = false
        end
           
        USER_OPTS[key[verb]][verb:upper()] = params
      end
    end  
  end
  cfg:close()
end

--
-- Write out a file if its contents have changed
--
local function writeFile( out_file, content )   

  local old_content = ""
  local s, hdr = pcall( io.input, out_file )

  if s then
    old_content = assert( hdr:read('*a') )
    hdr:close()
  end

  if content ~= old_content then
    hdr = assert( io.output(out_file.."a") )
    hdr:write( content)
    hdr:close()
  end

end

--
-- Write out the user_config.h file
--
local function generateUserConfig(pathname)

  local define, uo = {}, USER_OPTS
  local SET,VALUES,ENABLE_OMIT,SET_OMIT = uo.SET,uo.VALUES,uo.ENABLE_OMIT,uo.SET_OMIT
  local function add(s) define[#define+1] = s end

  if VALUES.FLASH_SIZE and FLASH_SIZE[VALUES.FLASH_SIZE] then
    SET['FLASH_' .. (VALUES.FLASH_SIZE) ]=true
  else
    SET.FLASH_AUTOSIZE = true
  end
  VALUES.FLASH_SIZE = nil
   
  for k,v in pairs(SET) do if v ~= FALSE then add("#define "..k) end end

  for k,v in pairs(VALUES) do add(("#define %s %s"):format(k,v)) end
  
  for k,v in pairs(ENABLE_OMIT) do if v == 1 then add(("#define %s_ENABLE"):format(k)) end end

  for k,v in pairs(SET_OMIT) do if v ~= FALSE and v ~= 0 then add(("#define %s %s"):format(k,tostring(v))) end end

  table.sort(define)
  writeFile( pathname .. 'user_config.h', USER_CONFIG:format(table.concat(define,"\n")) )   
end

--
-- Write out the user_modules.h file
--
local function generateUserIncludes(pathname)

  local define, BUILTIN, MODULES = {}, USER_OPTS.BUILTIN, USER_OPTS.MODULES
  local function add(p,s) define[#define+1] = p..s end

  assert( MODULES )

  for i = 1,#BUILTIN do add( CORE_PREFIX, BUILTIN[i]:upper() ) end
  
  add( "\n", "#ifndef LUA_CROSS_COMPILER" )

  for i = 1,#MODULES do add( MODULE_PREFIX, MODULES[i]:upper() ) end

  writeFile( pathname .. 'user_modules.h', USER_MODULES:format(table.concat(define,"\n")) )   

end

local filename = arg[1] or 'app/includes/user.cfg'
local pathname = ({filename:find("^(.*/)")})[3] or "./"
 
parseConfig(filename)
generateUserConfig(pathname)
generateUserIncludes(pathname)

