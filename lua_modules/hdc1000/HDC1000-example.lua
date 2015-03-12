HDC1000 = require("HDC1000")

sda = 1
scl = 2
drdyn = false

HDC1000.init(sda, scl, drdyn)
HDC1000.config() -- default values are used if called with no arguments. prototype is config(address, resolution, heater)

print(string.format("Temperature: %.2f °C\nHumidity: %.2f %%", HDC1000.getTemp(), HDC1000.getHumi()))

HDC1000 = nil
package.loaded["HDC1000"]=nil