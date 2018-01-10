--SAFETRIM
-- function _provision(self,socket,first_rec)
 
local self, socket, first_rec = ...
local crypto, file, json, node, table = crypto, file, sjson,   node, table
local stripdebug, gc = node.stripdebug, collectgarbage

local buf = {}
gc(); gc() 

local function getbuf()  -- upval: buf, table
  if #buf > 0 then return table.remove(buf, 1) end -- else return nil
end

-- Process a provisioning request record
local function receiveRec(socket, rec)  -- upval: self, buf, crypto
  -- Note that for 2nd and subsequent responses, we assme that the service has 
  -- "authenticated" itself, so any protocol errors are fatal and lkely to 
  -- cause a repeating boot, throw any protocol errors are thrown.
  local buf, config, file, log = buf, self.config, file, self.log
  local cmdlen = (rec:find('\n',1, true) or 0) - 1
  local cmd,hash = rec:sub(1,cmdlen-6), rec:sub(cmdlen-5,cmdlen)
  if cmdlen < 16 or
     hash ~= crypto.toHex(crypto.hmac("MD5",cmd,self.secret):sub(-3)) then
    return error("Invalid command signature")
  end

  local s; s, cmd = pcall(json.decode, cmd)
  local action,resp = cmd.a, {s = "OK"}
  local chunk

  if action == "ls" then
    for name,len in pairs(file.list()) do
      resp[name] = len
    end

  elseif action == "mv" then
    if file.exists(cmd.from) then
      if file.exists(cmd.to) then file.remove(cmd.to) end
      if not file.rename(cmd.from,cmd.to) then
        resp.s = "Rename failed"
      end
    end

  else
    if action == "pu" or action == "cm"  or action == "dl" then
      -- These commands have a data buffer appended to the received record
      if cmd.data == #rec - cmdlen - 1 then
        buf[#buf+1] = rec:sub(cmdlen +2)
      else
        error(("Record size mismatch, %u expected, %u received"):format(
              cmd.data or "nil", #buf - cmdlen - 1))
      end
    end

    if action == "cm" then
      stripdebug(2)
      local lcf,msg = load(getbuf, cmd.name)
      if not msg then
        gc(); gc()
        local code, name = string.dump(lcf), cmd.name:sub(1,-5) .. ".lc"
        local s = file.open(name, "w+")
        if s then
          for i = 1, #code, 1024 do
            s = s and file.write(code:sub(i, ((i+1023)>#code) and i+1023 or #code))
          end
          file.close()
          if not s then file.remove(name) end
        end
        if s then
          resp.lcsize=#code
          print("Updated ".. name)
        else 
          msg = "file write failed"
        end
     end
     if msg then
        resp.s, resp.err  = "compile fail", msg
     end
     buf = {}

    elseif action == "dl" then
      local s = file.open(cmd.name, "w+")
      if s then
        for i = 1, #buf do
           s = s and file.write(buf[i])
        end
        file.close()
      end
   
      if s then
        print("Updated ".. name)
      else 
        file.remove(name)
        resp.s = "write failed"
      end
      buf = {}
   
    elseif action == "ul" then
      if file.open(cmd.name, "r") then
        file.seek("set", cmd.offset)
        chunk = file.read(cmd.len)
        file.close()
      end

    elseif action == "restart" then
      cmd.a = nil
      cmd.secret = self.secret
      file.open(self.prefix.."config.json", "w+")
      file.writeline(json.encode(cmd))
      file.close()
      socket:close()
      print("Restarting to load new application")
      node.restart()  -- reboot just schedules a restart
      return
    end
  end
  self.socket_send(socket, resp, chunk)
  gc()
end

-- Replace the receive CB by the provisioning version and then tailcall this to
-- process this first record. 
socket:on("receive", receiveRec)
return receiveRec(socket, first_rec)
