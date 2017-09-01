# net_info Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-05-03 | [vsky279](https://github.com/vsky279) | [vsky279](https://github.com/vsky279) | [net_info.c](../../../app/modules/net_info.c)

This module is a wrapper for common network diagnostic and analysis tools.

## net_info Module Methods

### ni:ping()

This is a function to ping a server. A callback function is called when response is received.

#### Syntax
`ni:ping(ip_address, [count], callback)`

#### Parameters
- `domain` destination domain or IP address
- `count` number of ping packets to be sent (optional parameter, default value is 4)
- `callback(bytes, ipaddr, seqno, rtt)` callback function which is invoked when response is received where
    - `bytes` number of bytes received from destination server (0 means no response)
    - `ipaddr` destination serve IP address
    - `seqno` ICMP sequence number (does not increment when no response is received)
    - `rtt` round trip time in ms

If domain name cannot be resolved callback is invoked with  `bytes` parameter equal to 0 (i.e. no response) and `nil` values for all other parameters.
  
#### Returns
`nil`

#### Example
```lua
net_info.ping("www.nodemcu.com", function (b, ip, sq, tm) 
    if ip then print(("%d bytes from %s, icmp_seq=%d time=%dms"):format(b, ip, sq, tm)) else print("Invalid IP address") end 
  end)
```

Multiple pings can start in short sequence thought if the new ping overlaps with the previous one the first stops receiving answers, i.e.
```lua
function ping_resp(b, ip, sq, tm)
  print(string.format("%d bytes from %s, icmp_seq=%d time=%dms", b, ip, sq, tm))
end

net_info.ping("8.8.8.8", 4, ping_resp)
tmr.create():alarm(1000, tmr.ALARM_SINGLE, function() net_info.ping("8.8.4.4", 4, ping_resp) end)
```
gives
```
32 bytes from 8.8.8.8, icmp_seq=9 time=14ms
32 bytes from 8.8.8.8, icmp_seq=10 time=9ms
32 bytes from 8.8.4.4, icmp_seq=11 time=6ms
32 bytes from 8.8.4.4, icmp_seq=13 time=12ms
0 bytes from 8.8.8.8, icmp_seq=0 time=0ms
32 bytes from 8.8.4.4, icmp_seq=15 time=16ms
0 bytes from 8.8.8.8, icmp_seq=0 time=0ms
32 bytes from 8.8.4.4, icmp_seq=16 time=7ms
```
