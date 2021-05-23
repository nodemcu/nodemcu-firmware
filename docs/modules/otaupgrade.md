# OTA Upgrade module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-06-24 | [DiUS](https://github.com/DiUS), [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [otaupgrade.c](../../components/modules/otaupgrade.c)|

The OTA Upgrade module provides access to the IDF Over-The-Air Upgrade
support, enabling new application firmware to be applied and booted into.

This module is not concerned with where the new application comes from.
The choice of download source and method (e.g. https, tftp) is left to
the user, as is the trigger to start an upgrade. A common approach is
to have the device periodically check in with a central server and
compare a provided version number with the currently running version,
and if necessary kick off an upgrade.

In order to use the `otaupgrade` module, there must exist at least two
OTA partitions (type `app`, subtype `ota_0` / `ota_1`), as well as the
"otadata" partition (type `data`, subtype `ota`). The IDF implements
the typical "flip-flop" approach to upgrades, in that one of the
partitions hosts the running application, and the upgrade is downloaded
into the inactive partition and only when fully downloaded and verified
is it marked as bootable. This makes the system resilient to incomplete
upgrades, be it due to power-loss, interrupted downloads, or other such
things.

An example partition table for OTA might look like:
```
# Name,  Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
ota_0,    app,  ota_0,  0x10000,0x130000
ota_1,    app,  ota_1, 0x140000,0x130000
```

Depending on whether the installed boot loader has been built with or
without rollback support, the upgrade process itself has four or three
steps. Without rollback support, the steps are:

- `otaupgrade.commence()`
- feed the new application image into `otaupgrade.write(data)` in chunks
- `otaupgrade.complete(1)` to finalise and reboot into the new application

If the boot loader is built with rollback support, an extra step is needed
after the new application has booted (and been tested to be "good", by
whatever metric(s) the user chooses):

- `otaupgrade.accept()` to mark this image as valid, and allow it to be
  booted into again.

If a new firmware is not `accept()`ed before the device reboots, the
boot loader will switch back to the previous firmware version (provided
said boot loader is built with rollback support). A common test before
marking a new firmware as valid is to ensure the upgrade server can be
reached, on the basis that as long as the firmware can be remotely
upgraded, it's "good enough" to accept.

## otaupgrade.info()

The boot info and application state and version info can be queried with
this function. Typically it will be used to check the version of the
running application, to compare against a "desired" version in order
to decide whether an upgrade is required.

#### Syntax
`otaupgrade.info()`

#### Parameters
None.

#### Returns
A list of three values:

- the name of the partition of the running application
- the name of the partition currently marked for boot next (typically the
  same as the running application, but after `otaupgrade.complete()` it
  may point to a new application partition.
- a table whose keys are the names of OTA partitions and corresponding
  values are tables containing:
    - `state` one of `new`, `testing`, `valid`, `invalid`, `aborted` or
    possibly `undefined`. The values `invalid` and `aborted` largely
    mean the same things. See the IDF documentation for specifics.
    A partition in `testing` state needs to call `otaupgrade.accept()`
    if it wishes to become `valid`.
    - `name` the application name, typically "NodeMCU"
    - `date` the build date
    - `time` the build time
    - `version` the build version, as set by the *PROJECT_VER* variable
    during build
    - `secure_version` the secure version number, if secure boot is enabled
    - `idf_version` the IDF version

#### Example
```lua
boot_part, next_part, info = otaupgrade.info()
print("Booted: "..boot_part)
print("  Next: "..next_part)
for p,t in pairs(info) do
  print("@ "..p..":")
  for k,v in pairs(t) do
    print("    "..k..": "..v)
  end
end
print("Running version: "..info[boot_part].version)
```

## otaupgrade.commence()

Wipes the spare application partition and prepares to receive the new
application firmware.

If rollback support is enabled, note that the running application must
first be marked valid/accepted before it is possible to commence a
new OTA upgrade.

#### Syntax
`otaupgrade.commence()`

#### Parameters
None.

#### Returns
`nil`

A Lua error may be raised if the OTA upgrade cannot be commenced for some
reason (such as due to incorrect partition setup).


## otaupgrade.write(data)

Write a chunk of application firmware data to the correct partition and
location. Data must be streamed sequentially, the IDF does not support
out-of-order data as would be the case from e.g. bittorrent.

#### Syntax
`otaupgrade.write(data)`

#### Parameters
- `data` a string of binary data

#### Returns
`nil`

A Lua error may be raised if the data can not be written, e.g. due to the
data not being a valid OTA image (the IDF performs some checks in this
regard).


## otaupgrade.complete(reboot)

Finalises the upgrade, and optionally reboots into the new application
firmware right away.

#### Syntax
`otaupgrade.complete(reboot)`

#### Parameters
- `reboot` 1 to reboot into the new firmware immediately, nil to keep running

#### Returns
`nil`

A Lua error may be raised if the image does not pass validation, or no data
has been written to the image at all.

#### Example
```lua
-- Quick, dirty and totally insecure "push upgrade" for development use.
-- Use netcat to push a new firmware to a device:
--   nc -q 1 your-device-ip 9999 < build/NodeMCU.bin
--
osv = net.createServer()
osv:listen(9999, function(conn)
  print('Commencing OTA upgrade')
  local status, err = pcall(otaupgrade.commence)
  if err then
    print(err)
    conn:send(err)
    conn:close()
  end
  conn:on('receive', function(sck, data)
    status, err = pcall(function() otaupgrade.write(data) end)
    if err then
      print(err)
      conn:send(err)
      conn:close()
    end
  end)
  conn:on('disconnection', function()
    print('EOF, completing OTA')
    status, err = pcall(function() otaupgrade.complete(1) end)
    if err then
      print(err)
    end
  end)
end)

```

## otaupgrade.accept()

When the installed boot loader is built with rollback support, a new
application image is by default only booted once. During this "test run"
it can perform whatever checks is appropriate (like testing whether it
can still reach the update server), and if satisfied can mark itself
as valid. Without being marked valid, upon the next reboot the system
would "roll back" to the previous version instead.

#### Syntax
`otaupgrade.accept()`

#### Parameters
None.

#### Returns
`nil`


## otaupgrade.rollback()

A new firmware may decide that it is not performing as expected, and
request an explicit rollback to the previous version. If the call to this
function succeeds, the system will reboot without returning from the
call.

Note that it is also possible to roll back to a previous firmware
version even after the new version has called `otaupgrade.accept()`.

#### Syntax
`otaupgrade.rollback()`

#### Parameters
None.

#### Returns
Never. Either the system is rebooted, or a Lua error is raised (e.g. due
to there being no other firmware to roll back to).
