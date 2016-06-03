There are essentially three ways to build your NodeMCU firmware: cloud build service, Docker image, dedicated Linux environment (possibly VM).

**Building manually**

Note that the *default configuration in the C header files* (`user_config.h`, `user_modules.h`) is designed to run on all ESP modules including the 512 KB modules like ESP-01 and only includes general purpose interface modules which require at most two GPIO pins.

## Cloud Build Service
NodeMCU "application developers" just need a ready-made firmware. There's a [cloud build service](http://nodemcu-build.com/) with a nice UI and configuration options for them.

## Docker Image
Occasional NodeMCU firmware hackers don't need full control over the complete tool chain. They might not want to setup a Linux VM with the build environment. Docker to the rescue. Give [Docker NodeMCU build](https://hub.docker.com/r/marcelstoer/nodemcu-build/) a try.

## Linux Build Environment
NodeMCU firmware developers commit or contribute to the project on GitHub and might want to build their own full fledged build environment with the complete tool chain. There is a [post in the esp8266.com Wiki](http://www.esp8266.com/wiki/doku.php?id=toolchain#how_to_setup_a_vm_to_host_your_toolchain) that describes this.