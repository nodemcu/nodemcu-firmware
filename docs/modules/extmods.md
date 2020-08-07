
# External modules. Plugging your own C modules. **BETA**

Note: this feature is still in beta. Configuration files / API may change.

In order to make the most of NodeMCU, you will undoubtedly have to connect your ESP device to many different hardware modules, which will require you to write driver code in C and expose a Lua interface.

To make this easy, we have come up with the concept of "external modules". External modules allow you to refer to an external git repository containing the module, which will be downloaded/updated and built along the firmware. It is similar to git submodules, but without the complexity and having to alter the parent repository, while also adapted to the compilation requirements of NodeMCU and Lua.

## How to use external modules:

To use external modules, simply create an `extmods.ini` file in the repository root, with the following syntax:

```ini
[lua_mount_point]
url=<git repo URL>
ref=<branch name, tag or commit id>
...
```

Where:
* **lua_mount_point**: Name you want the referenced module to have in Lua.
* **url**: Repository URL where to fetch the code from
* **ref**: branch, tag or commit hash of the version of the module you want to pull in.
* **disabled**: (optional) Whether or not to actually compile the module (see below)

For example:

```ini
[helloworld]
url=git@github.com:espore-ide/nodemcu-module-helloworld.git
ref=master
```

You can add further sections to `extmods.ini`, one for each module you want to add. Once this file is ready, run the update command:

```shell
make extmod-update
```

This will download or update the modules to the external modules directory, `components/modules/external`.

You can now compile the firmware with `make` as usual. The build system will find your external modules and compile them along the core modules.

After this is flashed to the device, the module in the example will be available in lua as `helloworld`.

### Updating to the latest code

If your external module entry in `extmods.ini` points to a branch, e.g., `master`, you can update your local version to the latest code anytime by simply running `make extmod-update`.

### Temporarily disabling an external module

If you want to stop compiling and including a module in the build for any reason, without having to remove the entry from `extmods.ini`, simply add a `disabled=true` entry in the module section and run `make extmod-update`.

Example:
```ini
[helloworld]
url=https://github.com/espore-ide/nodemcu-module-helloworld.git
ref=master
disabled=true
```

### Mounting different module versions

Provided the module is well written (no global variables, etc), it is even possible to easily mount different versions of the same module simultaneously for testing:

```ini
[helloworld]
url=https://github.com/espore-ide/nodemcu-module-helloworld.git
ref=master

[helloworld_dev]
url=https://github.com/espore-ide/nodemcu-module-helloworld.git
ref=dev
```

Note that the second one points to a different branch, `dev`. Both modules will be visible in Lua under `helloworld` and `helloworld_dev` respectively.

## How to write external modules:

To write your own external module do the following:

1. Create an empty repository in your favorite remote, GitHub, BitBucket, GitLab, etc, or fork the helloworld example.
2. Create an entry in `extmods.ini` as explained above, with the `url=` key pointing to your repository. For modules that you author, it is recommended to use an updateable git URL in SSH format, such as `git@github.com:espore-ide/nodemcu-module-helloworld.git`.
3. Run `make extmod-update`

You can now change to `components/modules/external/your_module` and begin work. Since that is your own repository, you can work normally, commit, create branches, etc.

### External module scaffolding

External modules must follow a specific structure to declare the module in C. Please refer to the [helloworld.c](https://github.com/nodemcu/nodemcu-firmware/blob/dev-esp32/tools/example/helloworld.c) example, or use it as a template. In particular:

1. Include `module.h` 
2. Define a Module Function Map with name `module`
3. Define a `module_init` function
4. Include the module lua entries by adding a call to the `NODEMCU_MODULE_STD` macro

Here is a bare minimum module:
```c
#include "module.h"

// Module function map
LROT_BEGIN(module)
/* module-level functions go here*/
LROT_END(module, NULL, 0)

// module_init is invoked on device startup
static int module_init(lua_State* L) {
    // initialize your module, register metatables, etc
    return 0;
}

NODEMCU_MODULE_STD();  // define Lua entries
```
For a full example module boilerplate, check the [helloworld.c](https://github.com/nodemcu/nodemcu-firmware/blob/dev-esp32/tools/example/helloworld.c) file.


### Special makefile or compilation requirements

If your module has special makefile requirements, you can create a `module.mk` file at the root of your module repository. This will be executed during compilation.

### What is this "component.mk" file that appeared in my repo when running `make extmod-update` ?

This file is ignored by your repository. Do not edit or check it in!. This file contains the necessary information to compile your module along with the others.

Note that you don't even need to add this file to your `.gitignore`, since the `make extmod-update` operation configures your local copy to ignore it (via `.git/info/exclude`). 

## Further work:

* Support for per-module menuconfig (`Kconfig`). This is actually possible already, but need to work around potential config variable collisions in case two module authors happen to choose the same variable names.
* Module registry: Create an official repository of known external modules.
* Move all non-essential and specific hardware-related C modules currently in the NodeMCU repository to external modules, each in their own repository.
* Create the necessary scaffolding to facilitate writing modules that will work both in ESP8266 and ESP32.
* Port this work to the ESP8266 branch. Mostly, the scripts that make this possible could work in both branches directly.