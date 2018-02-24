--SAFETRIM
-- function _init(self, args)
  local self, args = ...
  
  -- The config is read from config.json but can be overridden by explicitly 
  -- setting the following args.  Setting to "nil" deletes the config arg.
  -- 
  --    ssid, spwd                Credentials for the WiFi
  --    server, port, secret      Provisioning server:port and signature secret
  --    leave                     If true then the Wifi is left connected
  --    espip, gw, nm, nsserver   These need set if you are not using DHCP

  local wifi, file, json, tmr = wifi, file, sjson, tmr
  local log, sta, config = self.log, wifi.sta, nil

  print ("\nStarting Provision Checks")
  log("Starting Heap:", node.heap())

  if file.open(self.prefix .. "config.json", "r") then
    local s; s, config = pcall(json.decode, file.read())
    if not s then print("Invalid configuration:", config) end
    file.close()
  end
  if type(config) ~= "table" then config = {} end

  for k,v in pairs(args or {}) do config[k] = (v ~= "nil" and v) end

  config.id = node.chipid()
  config.a  = "HI"

  self.config = config
  self.secret = config.secret
  config.secret = nil

  log("Config is:",json.encode(self.config))
  log("Mode is", wifi.setmode(wifi.STATION, false), config.ssid, config.spwd)
  log("Config status is", sta.config(
            { ssid = config.ssid, pwd  = config.spwd, auto = false, save = false } ))

  if config.espip then
    log( "Static IP setup:", sta.setip(
            { ip = config.espip, gateway = config.gw, netmask = config.nm }))
  end
  sta.connect(1)

  package.loaded[self.modname] = nil
  self.modname=nil
  tmr.alarm(0, 500, tmr.ALARM_AUTO, self:_doTick()) 
-- end
