-------------------------------------------------------------------------
-- Phant module for NodeMCU
-- Based on code from Sparkfun: https://github.com/sparkfun/phant-arduino
-- Tiago Custódio <tiagocustodio1@gmail.com>
-------------------------------------------------------------------------

local moduleName = ...
local M = {}
_G[moduleName] = M

local _pub;
local _prv;
local _host;
local _params;

function M.init(host, publicKey, privateKey)
	_host = host
	_pub = publicKey
	_prv = privateKey
	_params = ""
end

function M.add(field, data)
	_params = _params .. "&" .. field .. "=" .. tostring(data)
end

function M.queryString()
	return _params
end

function M.url()
	local result = "http://" .. _host .. "/input/" .. _pub .. ".txt" .. "?private_key=" .. _prv .. _params
	_params = ""
	return result;
end

function M.get(format)
	if format == nil then
		format = "csv"
	end
	local result = "GET /output/" .. _pub .. "." .. format .. " HTTP/1.1\n" .. "Host: " .. _host .. "\n" .. "Connection: close\n"
	return result
end

function M.post()
	local params = string.sub(_params, 2);
	local result = "POST /input/" .. _pub .. ".txt HTTP/1.1\n" .. "Host: " .. _host .. "\n"
	result = result .. "Phant-Private-Key: " .. _prv .. '\n'
	result = result .. "Connection: close\n"
	result = result .. "Content-Type: application/x-www-form-urlencoded\n"
	result = result .. "Content-Length: " .. string.len(params) .. "\n\n"
	result = result .. params

	_params = "";
	return result;
end

function M.clear()
	local result = "DELETE /input/" .. _pub .. ".txt HTTP/1.1\n"
	result = result .. "Host: " .. _host .. "\n"
	result = result .. "Phant-Private-Key: " .. _prv .."\n"
	result = result .. "Connection: close\n"

	return result;
end


return M
