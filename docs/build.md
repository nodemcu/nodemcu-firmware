#Building the firmware
There are essentially three ways to build your NodeMCU firmware: cloud build service, Docker image, dedicated Linux environment (possibly VM).

##Cloud build serivce
NodeMCU "application developers" just need a ready-made firmware. There's a [cloud build service](http://nodemcu-build.com/) with a nice UI and configuration options for them.

##Docker image
Occasional NodeMCU firmware hackers don't need full control over the complete tool chain. They might not want to setup a Linux VM with the build environment. Docker to the rescue. https://hub.docker.com/r/marcelstoer/nodemcu-build/

##Linux build environment
NodeMCU firmware developers commit or contribute to the project on GitHub and might want to build their own full fledged build environment with the complete tool chain. http://www.esp8266.com/wiki/doku.php?id=toolchain#how_to_setup_a_vm_to_host_your_toolchain