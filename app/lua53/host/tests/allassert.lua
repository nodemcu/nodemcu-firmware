local gif, assert = require'debug'.getinfo,assert
function _G.assert(exp,...)
  if exp then
    return exp,...
  else
    local msg = "Assert raised - lines: "
    for i = 2, 4 do
      local gi = gif(i)
      if type(gi) == "table" and gi.currentline > 0 then
	msg = msg .. gi.currentline .. " "
      else
	break
      end
    end
    print(msg)
  end
end
