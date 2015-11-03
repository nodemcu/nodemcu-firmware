-------------------------------------------------------------------------
-- Phant module for NodeMCU
-- Based on code from Sparkfun: https://github.com/sparkfun/phant-arduino
-- Tiago Custódio <tiagocustodio1@gmail.com>
-------------------------------------------------------------------------

require("phant")

hostName = "data.sparkfun.com"
publicKey = "VGb2Y1jD4VIxjX3x196z"
privateKey = "9YBaDk6yeMtNErDNq4YM"

phant.init(hostName, publicKey, privateKey)

phant.add("val1", "url")
phant.add("val2", 22)
phant.add("val3", 0.1234)

print("----TEST URL-----")
print(phant.url() .. "\n")

phant.add("val1", "post")
phant.add("val2", false)
phant.add("val3", 98.6)

print("----HTTP POST----")
print(phant.post() .. "\n")

print("----HTTP GET----")
print(phant.get())

print("----HTTP DELETE----")
print(phant.clear())

phant = nil
package.loaded["phant"] = nil
