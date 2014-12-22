--myfile.lua

local myfile = {}
function myfile.print(name)
  file.open(name)
  repeat
    local line=file.readline()
    if line then line=(string.gsub(line,"\n","")) print(line) end
  until not line
  file.close()
end
return myfile
