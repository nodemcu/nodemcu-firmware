# SDM Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-07-01 | [Matsievskiy S.V.](https://gitlab.com/matsievskiysv) | [Matsievskiy S.V.](https://gitlab.com/matsievskiysv) | [sdm](../../app/modules/sdm)|

This module provides driver abstraction layer to device functionality.
This is somewhat similar to [linux](https://wiki.linuxfoundation.org/tab/linux-device-driver-model) and [uboot](http://www.denx.de/wiki/U-Boot/DriverModel) driver models.

There's a [paper](https://gitlab.com/matsievskiysv/sdm-paper) that showcases sdm.
Code examples may be found [here](https://gitlab.com/matsievskiysv/sdm_driver).

## Model

Devices attached to chip may be represented in form of tree with buses as nodes and devices as leafs.

```
       +--------+
       |ESP8266 |
       +-+-+-+--+
         | | |
   +-----+ | +-----+
   |       |       |
+--v--+  +-v-+  +--v--+
|1WIRE|  |SPI|  |USART|
+-----+  +---+  +-----+
```

It's beneficial to encapsulate all device logic in one piece of code - driver.
This allows to reuse code for different instances of device.

```
             DEVICES                  +   DRIVERS
                                      |
             +-----+                  |   +-----+
             |1WIRE<----------------------+1WIRE|
             ++-+-++                  |   +-----+
              | | |                   |
    +---------+ | +--------+          |   +------+
    |           |          |       +------+DS1820|
+---v----+  +---v----+ +---v----+  |  |   +------+
|DS1820|0|  |DS1820|1| |DS1822|0|  |  |
+---^----+  +---^----+ +---^----+  |  |   +------+
    |           |          +--------------+DS1822|
    |           |                  |  |   +------+
    +-----------+------------------+  +
```

## sdm workflow

First step in using **sdm** is device tree initialization.
It is done using command `sdm.init()`.

Then, drivers and devices are added.
This will be discussed in detail later.

When drivers and devices are present, they are binded to each other
using `sdm.device_poll()` function.
This function iterates over drivers and uses driver internal logic to determine if selected driver is suitable for the device.

After initialization and binding, device functionality is exposed
via `attribute` and `method` interfaces.

Function `sdm.destroy()` frees all allocated memory and [removes references](https://www.lua.org/manual/5.1/manual.html#luaL_unref) to Lua objects, allowing them to be [collected](https://www.lua.org/manual/5.1/manual.html#2.10).

```
+--------------+
|    Library   |
|initialization|
+------+-------+
       |
       |
    +--v---+
    |Driver|
    |adding|
    +--+---+
       |
       |
    +--v---+
    |DeVice|
    |adding|
    +--+---+
       |
       |
 +-----v-------+
 |Device-driver|
 |   binding   |
 +-----+-------+
       |
       |
     +-v--+
     |Work|
     +-+--+
       |
       |
  +----v------+
  |Device tree|
  |destruction|
  +-----------+
```

#### Example:

This example maps some functions form [*NodeMCU* node module](https://nodemcu.readthedocs.io/en/master/en/modules/node/).
It may be run using *NodeMCU* interpreter without any modifications.

```Lua
sdm.init() -- init library

drv = sdm.driver_add("ESP8266") -- add driver
-- if run from script, this variable should be local

-- add _poll method for driver
sdm.method_add(drv, "_poll", nil,
               function(dev, drv, par)
                  return (sdm.device_basename(dev) == "ESP8266") and (par == nil)
               end
)

-- add heap method for driver
sdm.method_add(drv, "heap", "Free heap", function() return node.heap() end)

drv = nil -- remove reference

-- list all ESP8266 driver methods
for i,j in pairs(sdm.driver_methods(sdm.driver_handle("ESP8266"))) do
   print(i,j)
end

print("ESP8266 driver found: ", sdm.device_poll(sdm.root()))

-- retrieve method function
method_handle = sdm.method_dev_handle(sdm.root(), "heap")
method_func = sdm.method_func(method_handle)

-- run function
print(sdm.method_name(method_handle), method_func())

-- destroy sdm
sdm.destroy()
```

## Drivers

`sdm.driver_add(name)` function adds new driver with the unique name `name` and returns handle to it.

This handle is used to add methods and attributes to driver.
They will be discussed later.

Each driver may be bound to one or more devices.
It cannot be removed from sdm until its reference counter reached zero.

Driver cannot be renamed.

```
     +-----------------------------------------------------+
     |                                                     |
     |                                   +--------+        |
     |                                   |        |        |
+----v----+                              |    +---v----+   |
|Driver   |                              |    |Device|0|   |
+---------+                              |    +-----+--+   |
|name     |                              |          |      |
|refcount |                              +--------+ +------+
|methods  +-------------------------+    |        |        |
|attrs    |---------|               |    |    +---v----+   |
|attached +------------------------------+    |Device|1|   |
+---------+         |               |         +-----+--+   |
                    |               |               |      |
              +-----v-----+    +----v---+           +------+
              |Attribute  |    |Method  |
              |list       |    |list    |
              +-----------+    +--------+
              |Attribute|0|    |Method|0|
              |Attribute|1|    |Method|1|
              |Attribute|2|    |Method|2|
              +-----------+    +--------+

```

#### Example:

This example defines driver with many methods and attributes.

```Lua
sdm.init() -- init library

drv = sdm.driver_add("ESP8266") -- add driver
-- if run from script, this variable should be local

-- add _poll method for driver
sdm.method_add(drv, "_poll", nil,
               function(dev, drv, par)
                  return (sdm.device_basename(dev) == "ESP8266") and (par == nil)
               end
)

-- add heap method for driver
sdm.method_add(drv, "heap", "Free heap", function() return node.heap() end)

-- add random method for driver
sdm.method_add(drv, "random", "Random number", function() return node.random() end)

-- add id attribute for driver
sdm.attr_add(drv, "id", "Chip ID", 0,
             function(drv)
                local attr = sdm.attr_drv_handle(drv, "id")
                return sdm.attr_data(attr)
             end,
             nil
)

-- add number attribute for driver
sdm.attr_add(drv, "number", "Number storage", 0,
             function(drv)
                local attr = sdm.attr_drv_handle(drv, "number")
                return sdm.attr_data(attr)
             end,
             function(drv, data)
                local attr = sdm.attr_drv_handle(drv, "number")
                sdm.attr_set(attr, data)
             end
)

drv = nil -- remove reference

-- list all ESP8266 driver methods
for i,j in pairs(sdm.driver_methods(sdm.driver_handle("ESP8266"))) do
   print(i,j)
end

-- list all ESP8266 driver attributes
for i,j in pairs(sdm.driver_attrs(sdm.driver_handle("ESP8266"))) do
   print(i,j)
end

print("ESP8266 driver found: ", sdm.device_poll(sdm.root()))

-- destroy sdm
sdm.destroy()
```

## Device

`sdm.device_add(name, parent)` function adds new device with the unique name `name` to parent `parent` and returns handle to it.
Only `root` device is parenless and it is created upon library initialization.
Its handle can be obtained using `sdm.root()` function.
So, all *NodeMCU* bus device will be added using function
`sdm.device_add(name, sdm.root())`.

Until device is bound to driver, it is considered disabled.
In this state only local attributes can be added to device.
Local attributes should only contain information that cannot not be retrieved at runtime (like device ID, pin number etc).

After driver binding, device may use driver methods and attributes, create private copies of driver attributes.
When driver is released, private copies of driver attributes are destroyed, local attributes are preserved.

Device must release driver before it can be removed.

Devices may be renamed using `sdm.driver_rename()` function on condition that new name is free.

#### Example:

```Lua
sdm.init() -- init library

drv = sdm.driver_add("ESP8266") -- add driver
-- if run from script, this variable should be local

-- add _poll method for driver
sdm.method_add(drv, "_poll", nil,
               function(dev, drv, par)
                  return (sdm.device_basename(dev) == "ESP8266") and (par == nil)
               end
)

-- add heap method for driver
sdm.method_add(drv, "heap", "Free heap", function() return node.heap() end)

-- add random method for driver
sdm.method_add(drv, "random", "Random number", function() return node.random() end)

-- add id attribute for driver
sdm.attr_add(drv, "id", "Chip ID", 0,
             function(drv)
                local attr = sdm.attr_drv_handle(drv, "id")
                return sdm.attr_data(attr)
             end,
             nil
)

-- add number attribute for driver
sdm.attr_add(drv, "number", "Number storage", 0,
             function(drv)
                local attr = sdm.attr_drv_handle(drv, "number")
                return sdm.attr_data(attr)
             end,
             function(drv, data)
                local attr = sdm.attr_drv_handle(drv, "number")
                sdm.attr_set(attr, data)
             end
)

drv = nil -- remove reference

-- list all ESP8266 driver methods
for i,j in pairs(sdm.driver_methods(sdm.driver_handle("ESP8266"))) do
   print(i,j)
end

-- list all ESP8266 driver attributes
for i,j in pairs(sdm.driver_attrs(sdm.driver_handle("ESP8266"))) do
   print(i,j)
end

print("ESP8266 driver found: ", sdm.device_poll(sdm.root()))

-- destroy sdm
sdm.destroy()
```

## Device naming convention

Driver names are unique because they are named after hardware part names, which are unique.
It is, however, possible to have more then one device of the same type attached to the SoC.

For the purpose of distinguishing device instances, library
utilizes two part device naming scheme.

```
{basename}{split char}{suffix}
```

First part is called `basename`.
It is a hardware part name.

Second part is a name delimiter char.
It is defined as `DEVSPLTCHAR` macro and compiled into binary.
By default split char is `-`.
Since *NodeMCU* is an IOT device, it is better to select split char according to [URI naming recommendations](https://www.w3.org/Addressing/URL/4_URI_Recommentations.html).
Obviously, split character must not be present in basename or suffix for library functions to work correctly.

Some library helper functions are:

 - `sdm.device_basename()`: return basename and suffix of device
 - `sdm.request_name()`: given basename, generate unique device name

## Driver binding

For purpose of device driver binding, three special driver methods are used: `_poll`, `_init` and `_free`.
When `sdm.device_poll(device_handle)` function is called, library iterates over driver list, running special `_poll(device_handle, driver_handle, parent_handle)` function.
This function may use device name, its local attributes and its parent attributes in order to make a decision whether or not driver can handle device.
Library binds to device first driver, which `_poll()` function returned `true`.
After that, if driver has `_init()` method present, it is called with arguments `_init(dev_handle, drv_handle, parent_handle)`.
Upon driver release, `_free(dev_handle, drv_handle, parent_handle)` method is called (if exists).

#### Example:

This is a part of [DS18B20](https://gitlab.com/matsievskiysv/sdm_driver/blob/master/drv/DS18B20.lua) driver:

```Lua
sdm.method_add(
   drv, "_poll", nil,
   function(dev, drv, par)
      local attr = sdm.attr_data(sdm.local_attr_handle(dev, "id"))
      if attr == nil then return false end
      -- bind driver if its parent is ESP8266_1W and
      -- its ID begins with 0x28
      return (sdm.device_name(par) == "ESP8266_1W") and (attr:byte(1) == 0x28)
   end
)

sdm.method_add(drv, "_init", nil,
               function(dev, drv, par)
                  sdm.device_rename(dev, sdm.request_name("DS18B20"))
                  -- copy two attributes, making them private
                  sdm.attr_copy(dev, "temp")
                  sdm.attr_copy(dev, "precision")
                  -- call parent setup method
                  local met = sdm.method_dev_handle(par, "setup")
                  local func = sdm.method_func(met)
                  func(par, dev)
               end
)

sdm.method_add(drv, "_free", nil,
               function(dev, drv, par)
                  -- call parent free method
                  local met = sdm.method_dev_handle(par, "free")
                  local func = sdm.method_func(met)
                  func(par, dev)
               end
)
```

## Methods and attributes

Each driver may have methods and attributes.
Driver methods are functions shared across devices.
Driver attributes are data containers for boolean, numeric or string data.
Because it is frequently desired to attach some logic to
attribute data assignment and retrieval, attributes have getter and setter hooks (each is optional).

#### Example:

This is a part of [DS18B20](https://gitlab.com/matsievskiysv/sdm_driver/blob/master/drv/DS18B20.lua) driver:

```Lua
sdm.attr_add(drv, "precision", "Precision (9|10|11|12)", 12,
             function(dev, precision)
                -- get data from local storage
                local attr = sdm.attr_dev_handle(dev, "precision")
                return sdm.attr_data(attr)
             end,
             function(dev, precision)
                local par = sdm.device_parent(dev)
                local attr = sdm.attr_dev_handle(dev, "precision")
                local ex = sdm.method_func(sdm.method_dev_handle(par, "exchange"))
                local modes = {[9]=0x1f, [10]=0x3f, [11]=0x5f, [12]=0x7f}
                -- set hardware precision
                if modes[precision] ~= nil then
                   ex(par, dev, {0x4e, 0, 0, modes[precision]})
                   sdm.attr_set(attr, precision)
                end
             end
)
```

Driver attributes hold data, shared between all device instances.
Attributes may be copied to device, at which point they become private attributes.
Private attributes are modified on per device basis and do not modify their prototype, stored in driver.

#### Example:

This is a part of [DS18B20](https://gitlab.com/matsievskiysv/sdm_driver/blob/master/drv/DS18B20.lua) driver:

```Lua
sdm.method_add(drv, "_init", nil,
          function(dev, drv, par)
          sdm.device_rename(dev, sdm.request_name("DS18B20"))
          -- copy two attributes, making them private
          sdm.attr_copy(dev, "temp")
          sdm.attr_copy(dev, "precision")
          -- call parent setup method
          local met = sdm.method_dev_handle(par, "setup")
          local func = sdm.method_func(met)
          func(par, dev)
          end
)
```

Third kind of attributes is local attribute.
It is an attribute, that stored inside device.
It holds data, not accessible programmatically.
Like device id or pin number, device is attached to.

| Property | Local attribute | Private attribute | Driver (public) attribute |
| --- |: --- :|: --- :|: --- :|
| Stored in | device | device | driver |
| Accessible using driver handle | - | - | + |
| Accessible using device handle | + | + | + |
| Shared between devices | - | - | + |
| Persist upon driver detach | + | - | + |

# Additional notes

## Device tree structure

[Device trees](https://www.devicetree.org/) are widely used to describe hardware.
Simple script may be used to imitate this kind of functionality.

#### Example:

```Lua
local root={
   local_attributes={},
   children={
      {
         name="ESP8266_1W",
         local_attributes={},
         children = {
            {
               name="DS18S20-0",
               local_attributes={
                  {
                     name="id",
                     desc=nil,
                     data=string.char(16) ..
                        string.char(221) ..
                        string.char(109) ..
                        string.char(104) ..
                        string.char(3) ..
                        string.char(8) ..
                        string.char(0) ..
                        string.char(150)
                  }
               }
            },
            {
               name="DS18B20-0",
               local_attributes={}
            }
         }
      },
      {
         name="ESP8266_SPI",
         local_attributes={},
         children = {
            {
               name="MCP3208-0"
            },
         }
      },
   }
}
```

## Pin management

Library supports simple pin management.
It is done via three commands:

 - `sdm.pin_request(device_handle, pin_number)` to bind pin to device,
 - `sdm.pin_free(pin_number)` to unbind device from pin,
 - `sdm.pin_device(pin_number)` to get device, binded to pin.

Note, that binding device to pin does not impose any restrictions on underlying hardware.
This feature is cooperative in nature.

Allowed pin number range is [0, `PINNUM`), where `PINNUM` is a C macro
set at compile time. By default it is 9.

## Locking mechanism

**sdm** device modes is basically a class - data structure with associated methods.
*NodeMCU* SoC is a multitasking device.
Using shared data structure in a multitasking context require some sort of locking mechanism.

Because chip lacks atomic compare-write command, true mutex cannot be constructed.
Library uses the next best thing - data structure lock bits.
They do no guarantee serialized access, but significantly reduce probability of context switch related bugs.

Each library function is wrapped in `lock_read()`/`unlock_read()` or `lock_write()`/`unlock_write()` pair.

Library may have unlimited number of readers or **one** writer at all times.

## Commands

### sdm.attr_add()

Add driver attribute.

#### Syntax

`sdm.attr_add(driver, name, description, data, getter, setter)`

#### Parameters

- `driver` driver handle
- `name` attribute name
- `description` attribute short description. `nil` for empty description
- `data` attribute data. Boolean, numeric or string
- `getter` attribute getter function
- `setter` attribute setter function

#### Returns

- new attribute handle or `nil`

### sdm.attr_copy()

Copy attribute to device.

#### Syntax

`sdm.attr_copy(device, name)`

#### Parameters

- `device` device handle
- `name` attribute name

#### Returns

- `true`: success or `false`: error

### sdm.attr_data()

Get attribute data.

#### Syntax

`sdm.attr_data(attr)`

#### Parameters

- `attr` attribute handle

#### Returns

- attribute data or `nil`

### sdm.attr_desc()

Get attribute description.

#### Syntax

`sdm.attr_desc(attr)`

#### Parameters

- `attr` attribute handle

#### Returns

- attribute description or `nil`

### sdm.attr_dev_handle()

Get attr handle.

#### Syntax

`sdm.attr_dev_handle(device, name)`

#### Parameters

- `device` device handle
- `name` attribute name

#### Returns

- attr handle or `nil`

### sdm.attr_drv_handle()

Get driver attribute handle.

#### Syntax

`sdm.attr_drv_handle(driver, name)`

#### Parameters

- `driver` driver handle
- `name` attribute name

#### Returns

- driver attribute handle or `nil`

### sdm.attr_func()

Get attribute functions.

#### Syntax

`sdm.attr_func(attr)`

#### Parameters

- `attr` attribute handle

#### Returns

- attribute getter function or `nil`
- attribute setter function or `nil`

### sdm.attr_handle()

Get attr handle.
Try finding attribute using `sdm.attr_dev_handle()`.
If not found, try `sdm.attr_drv_handle()`.

#### Syntax

`sdm.attr_handle(device, name)`

#### Parameters

- `device` device handle
- `name` attribute name

#### Returns

- attr handle or `nil`

### sdm.attr_name()

Get attribute name.

#### Syntax

`sdm.attr_name(attr)`

#### Parameters

- `attr` attribute handle

#### Returns

- attribute name or `nil`

### sdm.attr_remove()

Remove attribute from driver.

#### Syntax

`sdm.attr_remove(driver, attr)`

#### Parameters

- `driver` driver handle
- `attr` attribute handle

#### Returns

- `true`: success or `false`: error

### sdm.attr_set()

Set attribute value.

#### Syntax

`sdm.attr_set(attr, data)`

#### Parameters

- `attr` attribute handle
- `data` attribute data. Boolean, numeric or string

#### Returns

- attribute handle or `nil`

### sdm.destroy()

Destroy library data structures.
Free sdm tree and remove references to all lua functions.

#### Syntax

`sdm.destroy()`

### sdm.device_children()

Device children.

#### Syntax

`sdm.device_children(device)`

#### Parameters

- `device` device handle

#### Returns

- children table or nil. table format: { name = handle , ...}

### sdm.device_local_attrs()

Get table of device local attributes.

#### Syntax

`sdm.device_local_attrs(device)`

#### Parameters

- `device` device handle

#### Returns

- local attribute table or nil. table format: { attrname = { [ desc = description ,] [ get = function ,] [ set = function ] }, ...}

### sdm.device_methods()

Get table of device methods.

#### Syntax

`sdm.device_methods(device)`

#### Parameters

- `device` device handle

#### Returns

- method table or nil. table format: { methodname = { [ desc = description ,] func = function }, ...}

### sdm.device_prvt_attrs()

Get table of device private attributes.

#### Syntax

`sdm.device_prvt_attrs(device)`

#### Parameters

- `device` device handle

#### Returns

- private attribute table or nil. table format: { attrname = { [ desc = description ,] [ get = function ,] [ set = function ] }, ...}

### sdm.devices()

Get table of devices.

#### Syntax

`sdm.devices()`

#### Returns

- device table or nil. table format: { devicename = handle, ...}

### sdm.driver_add()

Add driver.

#### Syntax

`sdm.driver_add(name)`

#### Parameters

- `name` name of new driver

#### Returns

- new driver handle or `nil`

### sdm.driver_attrs()

Get table of driver attributes.

#### Syntax

`sdm.driver_attrs(driver)`

#### Parameters

- `driver` driver handle

#### Returns

- attribute table or nil. table format: {attrname = { [ desc = description ,] [ get = function,] [ set= function ] }, ...}

### sdm.driver_handle()

Get driver handle.

#### Syntax

`sdm.driver_handle(name)`

#### Parameters

- `name` name of driver

#### Returns

- new driver handle or `nil`

### sdm.driver_methods()

Get table of driver methods.

#### Syntax

`sdm.driver_methods(driver)`

#### Parameters

- `driver` driver handle

#### Returns

- method table or nil. table format: {methodname = { [ desc= description,] func = function}, ...}

### sdm.driver_name()

Get driver name.

#### Syntax

`sdm.driver_name(driver)`

#### Parameters

- `driver` driver handle

#### Returns

- driver name or `nil`

### sdm.driver_prune()

Remove unused drivers.

#### Syntax

`sdm.driver_prune()`

#### Returns

- `true`: success or `false`: error

### sdm.driver_remove()

Remove driver.

#### Syntax

`sdm.driver_remove(name)`

#### Parameters

- `name` driver name

#### Returns

- `true`: success or `false`: error

### sdm.drivers()

Get list of drivers.

#### Syntax

`sdm.drivers()`

#### Returns

- driver table or nil. table format: {name = handle, ...}

### sdm.handle_type()

Get type of handle.

#### Syntax

`sdm.handle_type(handle)`

#### Parameters

- `handle` lua [light user data](https://www.lua.org/manual/5.3/manual.html#lua_pushlightuserdata).
this may be handle of any type

#### Returns

- strings `driver` or `device` or `method` or `attribute` or `unknown` or `nil`

### sdm.init()

Initialize library.

#### Syntax

`sdm.init()`

### sdm.local_attr_add()

Add local device attribute.

#### Syntax

`sdm.local_attr_add(device, name, description, data, getter, setter)`

#### Parameters

- `device` device handle
- `name` attribute name
- `description` attribute short description. `nil` for empty description
- `data` attribute data. Boolean, numeric or string
- `getter` attribute getter function
- `setter` attribute setter function

#### Returns

- new attribute handle or `nil`

### sdm.local_attr_handle()

Get local device attribute handle.

#### Syntax

`sdm.local_attr_handle(device, name)`

#### Parameters

- `device` device handle
- `name` attribute name

#### Returns

- device attribute handle or `nil`

### sdm.local_attr_remove()

Remove local attribute from device.

#### Syntax

`sdm.local_attr_remove(device, attr)`

#### Parameters

- `device` device handle
- `attr` attribute handle

#### Returns

- `true`: success or `false`: error

### sdm.method_add()

Add new method to driver.

#### Syntax

`sdm.method_add(driver, name, description, function)`

#### Parameters

- `driver` driver handle
- `name` method name
- `description` method short description. nil for empty description
- `function` function reference

#### Returns

- new method handle or `nil`

### sdm.method_desc()

Get method description.

#### Syntax

`sdm.method_desc(met)`

#### Parameters

- `met` method handle

#### Returns

- method description or `nil`

### sdm.method_drv_handle()

Get driver method handle.

#### Syntax

`sdm.method_drv_handle(driver, name)`

#### Parameters

- `driver` driver handle
- `name` method name

#### Returns

- driver method handle or `nil`

### sdm.method_func()

Get method function.

#### Syntax
`sdm.method_func(met)`

#### Parameters

- `met` method handle

#### Returns

- method function or `nil`

### sdm.method_name()

Get method name.

#### Syntax

`sdm.method_name(met)`

#### Parameters

- `met` method handle

#### Returns

- method name or `nil`

### sdm.method_remove()

Remove method from driver.

#### Syntax

`sdm.method_remove(driver, met)`

#### Parameters

- `driver` driver handle
- `met` method handle

#### Returns

- `true`: success or `false`: error

### sdm.pin_device()

Get device, associated with pin.

#### Syntax

`sdm.pin_device(pin_number)`

#### Parameters

- `pin_number` pin_number

#### Returns

- device handle or `nil`

### sdm.pin_free()

Free pin from device.
Pin number range is [0,`PINNUM`).
By default `PINNUM` = 9

#### Syntax

`sdm.pin_free(pin_number)`

#### Parameters

- `pin_number` pin_number

#### Returns

- `true`: success or `false`: error

### sdm.pin_request()

Associate pin with device.
Pin number range is [0,`PINNUM`).
By default `PINNUM` = 9

#### Syntax

`sdm.pin_request(device, pin_number)`

#### Parameters

- `device` device handle
- `pin_number` pin_number

#### Returns

- `true`: success or `false`: error

### sdm.prvt_attr_handle()

Get attr handle.

#### Syntax

`sdm.prvt_attr_handle(device, name)`

#### Parameters

- `device` device handle
- `name` attribute name

#### Returns

- attr handle or `nil`

### sdm.prvt_attr_remove()

Remove private attribute from device.

#### Syntax

`sdm.prvt_attr_remove(device, attr)`

#### Parameters

- `device` device handle
- `attr` attribute handle

#### Returns

- `true`: success or `false`: error

### sdm.request_name()

Get avaliable device name given basename.

#### Syntax

`sdm.request_name(basename)`

#### Parameters

- `basename` device basename

#### Returns

- avaliable device name or `nil`

### sdm.root()

Get sdm root device.

#### Syntax

`sdm.root()`

#### Returns

- root device handle or `nil`
