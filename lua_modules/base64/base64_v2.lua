--this version actually works

local tab = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
base64 = {
	enc = function(data)
		local l,out = 0,''
		local m = (3-data:len()%3)%3
		local d = data..string.rep('\0',m)
		for i=1,d:len() do
			l = bit.lshift(l,8)
			l = l+d:byte(i,i)
			if i%3==0 then
				for j=1,4 do
					local a = bit.rshift(l,18)+1
					out = out..tab:sub(a,a)
					l = bit.band(bit.lshift(l,6),0xFFFFFF)
				end
			end
		end
		return out:sub(1,-1-m)..string.rep('=',m)
	end,
	dec = function(data)
		local a,b = data:gsub('=','A')
		local out = ''
		local l = 0
		for i=1,a:len() do
			l=l+tab:find(a:sub(i,i))-1
			if i%4==0 then 
				out=out..string.char(bit.rshift(l,16),bit.band(bit.rshift(l,8),255),bit.band(l,255))
				l=0
			end
			l=bit.lshift(l,6)
		end
		return out:sub(1,-b-1)
	end
}
