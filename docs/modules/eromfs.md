# EROMFS Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2021-11-13 | [Johny Mattsson](https://github.com/jmattsson) |[Johny Mattsson](https://github.com/jmattsson) | [heaptrace.c](../../components/modules/eromfs.c)|

EROMFS (Embedded Read-Only Mountable File Sets) provides a convenient mechanism
for bundling file sets into the firmware image itself. The main use cases
envisaged for this is static web site content and default "skeleton" files
that may be used to populate SPIFFS on first boot.

When enabling the `eromfs` module one or more file sets ("volumes") must be
declared. Each such volume is identified by name, and may be mounted anywhere
supported by the [Virtual File System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/vfs.html). Once mounted, the included
files are available on a read-only basis to any thread wanting to access them.

Note that EROMFS does not support directories per se, but will store the
directory path as part of the filename just as SPIFFS does. As such it is
only possible to list the root of the volume, not subdirectories (since
they don't exist).

## eromfs.list
Returns a list of the bundled file sets (volumes).

#### Syntax
```lua
eromfs.list()
```

#### Parameters
None.

#### Returns
An array with the names of the bundled volumes.

#### Example
```lua
for _, volname in ipairs(eromfs.list()) do print(volname) end
```

## eromfs.mount
Mounts a volume at a specified point in the virtual file system.

Note that it is technically possible to mount a volume multiple times on
different mount points. The benefit of doing so however is questionable.

#### Syntax
```lua
eromfs.mount(volname, mountpt)
```

#### Parameters
- `volname` the name of the volume to mount, e.g. `myvol`.
- `mountpt` where to mount said volume. Must start with '/', e.g. `/myvol`.

#### Returns
`nil` on success. Raises an error if the named volume cannot be found, or
cannot be mounted.

#### Example
```lua
-- Assumes the volume named "myvol" exists
eromfs.mount('myvol', '/somewhere')
for name,size in pairs(file.list('/somewhere')) do print(name, size) end
```

## eromfs.unmount
Unmounts the specified EROMFS volume from the given mount point.

#### Syntax
```lua
eromfs.unmount(volname, mountpt)
```

#### Parameters
- `volname` the name of the volume to mount.
- `mountpt` the current mount point of the volume.

#### Returns
`nil` if:
- the volume was successfully unmounted; or
- the volume was not currently mounted at the given mount point

Raises an error if:
- the unmounting fails for some reason; or
- a different EROMFS volume is mounted on the given mount point

#### Example
```lua
eromfs.unmount('myvol', '/somewhere')
```
