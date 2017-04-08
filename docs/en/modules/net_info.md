# net_info Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-01-21 | [smcl](http://blog.mclemon.io/esp8266-contributing-to-the-nodemcu-ecosystem) | [wolfgangr](https://github.com/wolfgangr/nodemcu-firmware) | [net_info.c](../../../app/modules/net_info.c)


This module is a stub to collect common network diagnostic and analysis tools.

##net_info.ping()
 send ICMP ECHO_REQUEST to network hosts 
 
**Synopsis:**:
```
net_info.ping(host [, count [, callback]]) 
```

**Usage example:**
```
=net_info.ping("www.google.de",2)
> 32 bytes from 172.217.20.195, icmp_seq=25 ttl=53 time=37ms
32 bytes from 172.217.20.195, icmp_seq=26 ttl=53 time=38ms
ping 2, timeout 0, total payload 64 bytes, 1946 ms
```


**Description:** (from *linux man 8 ping*)

> `ping` uses the ICMP protocol's mandatory ECHO_REQUEST datagram to elicit an ICMP ECHO_RESPONSE from a host or gateway. ECHO_REQUEST datagrams (''pings'') have an IP and ICMP header, followed by a struct timeval and then an arbitrary number of ''pad'' bytes used to fill out the packet. 


**Usage variants**

ping host **by IP-Adress:**  
```
net_info.ping("1.2.3.4")
```
Enter IP-Adress in commonly known dotted quad-decimal notation, enclosed in string quotes.

!!! Note
    There is only limited error checking on the validity of IP adresses in lwIP. Check twice!


Use **DNS resolver** to get IP adress for ping:
```
net_info.ping("hostname")
```

!!! Note
    This only works with Network access and DNS resolution properly configured.

    
Custom **ping count**:
```
net_info.ping(host, count)
```
* `host` can be given by IP or hostname as above.
* `count` number of repetitive pings - default is 4 if omitted.


Ping **callback function**:
```
net_info.ping(host, count, callback)
```
Instead of printing ping results on the console, the given callback function ist called each time a ECHO_RESPONSE is received.

The callback receives the following arguments:  
```
function ping_recv(bytes, ipaddr, seqno, ttl, ping)
```
* length of message
* ip-address of pinged host
* icmp_seq number
* time-to-live-value
* ping time in ms

!!! Caution
    The callback functionality is still untested. Use at even more your own risk!

For further reference to callback functionality, see smcl origin link provided on top of this page.