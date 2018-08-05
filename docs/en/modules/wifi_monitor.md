# WiFi.monitor Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-12-20 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [wifi_monitor.c](../../../app/modules/wifi_monitor.c)|

This is an optional module that is only included if `LUA_USE_MODULES_WIFI_MONITOR` is defined in the `user_modules.h` file. This module
provides access to the monitor mode features of the ESP8266 chipset. In particular, it provides access to received WiFi management frames.

This module is not for casual use -- it requires an understanding of IEEE802.11 management protocols.

## wifi.monitor.start()

This registers a callback function to be called whenever a management frame is received. Note that this can be at quite a high rate, so some limited
filtering is provided before the callback is invoked. Only the first 110 bytes or so of the frame are returned -- this is an SDK restriction.
Any connected AP/station will be disconnected. Calling this function sets the channel back to 1.

#### Syntax
`wifi.monitor.start([filter parameters,] mgmt_frame_callback)`

#### Parameters
- filter parameters. This is a byte offset (1 based) into the underlying data structure, a value to match against, and an optional mask to use for matching.
  The data structure used for filtering is 12 bytes of [radio header](#the-radio-header), and then the actual frame. The first byte of the frame is therefore numbered 13. The filter
  values of 13, 0x80 will just extract beacon frames.
- `mgmt_frame_callback` is a function which is invoked with a single argument which is a `wifi.packet` object which has many methods and attributes.


#### Returns
nothing.

#### Example
```
wifi.monitor.start(13, 0x80, function(pkt)
    print ('Beacon: ' .. pkt.bssid_hex .. " '" .. pkt[0] .. "' ch " .. pkt[3]:byte(1))
end)
wifi.monitor.channel(6)
```

## wifi.monitor.stop()

This disables the monitor mode and returns to normal operation. There are no parameters and no return value.

#### Syntax
`wifi.monitor.stop()`

## wifi.monitor.channel()

This sets the channel number to monitor. Note that in many applications you will want to step through the channel numbers at regular intervals. Beacon
frames (in particular) are typically sent every 102 milliseconds, so a switch time of (say) 150 milliseconds seems to work well.
Note that this function should be called after starting to monitor, since `wifi.monitor.start` resets the channel back to 1.

#### Syntax
`wifi.monitor.channel(channel)`

#### Parameters
- `channel` sets the channel number in the range 1 to 15.

#### Returns
nothing.

# wifi.packet object

This object provides access to the raw packet data and also many methods to extract data from the packet in a simple way.

## packet:radio_byte()

This is like the `string.byte` method, except that it gives access to the bytes of the [radio header](#the-radio-header).

#### Syntax
`packet:radio_byte(n)`

#### Parameters
- `n` the byte number (1 based) to get from the [radio header](#the-radio-header) portion of the packet

#### Returns
0-255 as the value of the byte
nothing if the offset is not within the [radio header](#the-radio-header).

## packet:frame_byte()

This is like the `string.byte` method, except that it gives access to the bytes of the received frame.

#### Syntax
`packet:frame_byte(n)`

#### Parameters
- `n` the byte number (1 based) to get from the received frame.

#### Returns
0-255 as the value of the byte
nothing if the offset is not within the received frame.


## packet:radio_sub()

This is like the `string.sub` method, except that it gives access to the bytes of the [radio header](#the-radio-header).

#### Syntax
`packet:radio_sub(start, end)`

#### Parameters
Same rules as for `string.sub` except that it operates on the [radio header](#the-radio-header).

#### Returns
A string according to the `string.sub` rules.

## packet:frame_sub()

This is like the `string.sub` method, except that it gives access to the bytes of the received frame.

#### Syntax
`packet:frame_sub(start, end)`

#### Parameters
Same rules as for `string.sub` except that it operates on the received frame.

#### Returns
A string according to the `string.sub` rules.


## packet:radio_subhex()

This is like the `string.sub` method, except that it gives access to the bytes of the [radio header](#the-radio-header). It also
converts them into hex efficiently.

#### Syntax
`packet:radio_subhex(start, end [, seperator])`

#### Parameters
Same rules as for `string.sub` except that it operates on the [radio header](#the-radio-header).
- `seperator` is an optional sting which is placed between the individual hex pairs returned.

#### Returns
A string according to the `string.sub` rules, converted into hex with possible inserted spacers.

## packet:frame_sub()

This is like the `string.sub` method, except that it gives access to the bytes of the received frame.

#### Syntax
`packet:frame_subhex(start, end [, seperator])`

#### Parameters
Same rules as for `string.sub` except that it operates on the received frame.
- `seperator` is an optional sting which is placed between the individual hex pairs returned.

#### Returns
A string according to the `string.sub` rules, converted into hex with possible inserted spacers.


## packet:ie_table()

This returns a table of the information elements from the management frame. The table keys values are the
information element numbers (0 - 255). Note that IE0 is the SSID. This method is mostly only useful if
you need to determine which information elements were in the management frame.

#### Syntax
`packet:ie_table()`

#### Parameters
None.

#### Returns
A table with all the information elements in it.

#### Example
```
print ("SSID", packet:ie_table()[0])
```

Note that this is possibly the worst way of getting the SSID. 

#### Alternative

The `packet` object itself can be indexed to extract the information elements.

#### Example
```
print ("SSID", packet[0])
```

This is more efficient than the above approach, but requires you to remember that IE0 is the SSID.

## packet.&lt;attribute&gt;

The packet object has many attributes on it. These allow easy access to all the fields, though not an easy way to enumerate them. All integers are unsigned
except where noted. Information Elements are only returned if they are completely within the captured frame. This can mean that for some frames, some of the
information elements can be missing.

When a string is returned as the value of a field, it can (and often is) be a binary string with embedded nulls. All information elements are returned as strings
even if they are only one byte long and look like a number in the specification. This is purely to make the interface consistent. Note that even SSIDs can contain
embedded nulls. 

|  Attribute name  |  Type  |
|:--------------------|:-------:|
| aggregation | Integer |
| ampdu_cnt | Integer |
| association_id | Integer |
| authentication_algorithm | Integer |
| authentication_transaction | Integer |
| beacon_interval | Integer |
| beacon_interval | Integer |
| bssid | String |
| bssid_hex | String |
| bssidmatch0 | Integer |
| bssidmatch1 | Integer |
| capability | Integer |
| channel | Integer |
| current_ap | String |
| cwb | Integer |
| dmatch0 | Integer |
| dmatch1 | Integer |
| dstmac | String |
| dstmac_hex | String |
| duration | Integer |
| fec_coding | Integer |
| frame | String (the entire received frame) |
| frame_hex | String |
| fromds | Integer |
| header | String (the fixed part of the management frame) |
| ht_length | Integer |
| ie_20_40_bss_coexistence | String |
| ie_20_40_bss_intolerant_channel_report | String |
| ie_advertisement_protocol | String |
| ie_aid | String |
| ie_antenna | String |
| ie_ap_channel_report | String |
| ie_authenticated_mesh_peering_exchange | String |
| ie_beacon_timing | String |
| ie_bss_ac_access_delay | String |
| ie_bss_available_admission_capacity | String |
| ie_bss_average_access_delay | String |
| ie_bss_load | String |
| ie_bss_max_idle_period | String |
| ie_cf_parameter_set | String |
| ie_challenge_text | String |
| ie_channel_switch_announcement | String |
| ie_channel_switch_timing | String |
| ie_channel_switch_wrapper | String |
| ie_channel_usage | String |
| ie_collocated_interference_report | String |
| ie_congestion_notification | String |
| ie_country | String |
| ie_destination_uri | String |
| ie_diagnostic_report | String |
| ie_diagnostic_request | String |
| ie_dms_request | String |
| ie_dms_response | String |
| ie_dse_registered_location | String |
| ie_dsss_parameter_set | String |
| ie_edca_parameter_set | String |
| ie_emergency_alart_identifier | String |
| ie_erp_information | String |
| ie_event_report | String |
| ie_event_request | String |
| ie_expedited_bandwidth_request | String |
| ie_extended_bss_load | String |
| ie_extended_capabilities | String |
| ie_extended_channel_switch_announcement | String |
| ie_extended_supported_rates | String |
| ie_fast_bss_transition | String |
| ie_fh_parameter_set | String |
| ie_fms_descriptor | String |
| ie_fms_request | String |
| ie_fms_response | String |
| ie_gann | String |
| ie_he_capabilities | String |
| ie_hopping_pattern_parameters | String |
| ie_hopping_pattern_table | String |
| ie_ht_capabilities | String |
| ie_ht_operation | String |
| ie_ibss_dfs | String |
| ie_ibss_parameter_set | String |
| ie_interworking | String |
| ie_link_identifier | String |
| ie_location_parameters | String |
| ie_management_mic | String |
| ie_mccaop | String |
| ie_mccaop_advertisement | String |
| ie_mccaop_advertisement_overview | String |
| ie_mccaop_setup_reply | String |
| ie_mccaop_setup_request | String |
| ie_measurement_pilot_transmission | String |
| ie_measurement_report | String |
| ie_measurement_request | String |
| ie_mesh_awake_window | String |
| ie_mesh_channel_switch_parameters | String |
| ie_mesh_configuration | String |
| ie_mesh_id | String |
| ie_mesh_link_metric_report | String |
| ie_mesh_peering_management | String |
| ie_mic | String |
| ie_mobility_domain | String |
| ie_multiple_bssid | String |
| ie_multiple_bssid_index | String |
| ie_neighbor_report | String |
| ie_nontransmitted_bssid_capability | String |
| ie_operating_mode_notification | String |
| ie_overlapping_bss_scan_parameters | String |
| ie_perr | String |
| ie_power_capability | String |
| ie_power_constraint | String |
| ie_prep | String |
| ie_preq | String |
| ie_proxy_update | String |
| ie_proxy_update_confirmation | String |
| ie_pti_control | String |
| ie_qos_capability | String |
| ie_qos_map_set | String |
| ie_qos_traffic_capability | String |
| ie_quiet | String |
| ie_quiet_channel | String |
| ie_rann | String |
| ie_rcpi | String |
| ie_request | String |
| ie_ric_data | String |
| ie_ric_descriptor | String |
| ie_rm_enabled_capacities | String |
| ie_roaming_consortium | String |
| ie_rsn | String |
| ie_rsni | String |
| ie_schedule | String |
| ie_secondary_channel_offset | String |
| ie_ssid | String |
| ie_ssid_list | String |
| ie_supported_channels | String |
| ie_supported_operating_classes | String |
| ie_supported_rates | String |
| ie_tclas | String |
| ie_tclas_processing | String |
| ie_tfs_request | String |
| ie_tfs_response | String |
| ie_tim | String |
| ie_tim_broadcast_request | String |
| ie_tim_broadcast_response | String |
| ie_time_advertisement | String |
| ie_time_zone | String |
| ie_timeout_interval | String |
| ie_tpc_report | String |
| ie_tpc_request | String |
| ie_tpu_buffer_status | String |
| ie_ts_delay | String |
| ie_tspec | String |
| ie_uapsd_coexistence | String |
| ie_vendor_specific | String |
| ie_vht_capabilities | String |
| ie_vht_operation | String |
| ie_vht_transmit_power_envelope | String |
| ie_wakeup_schedule | String |
| ie_wide_bandwidth_channel_switch | String |
| ie_wnm_sleep_mode | String |
| is_group | Integer |
| legacy_length | Integer |
| listen_interval | Integer |
| mcs | Integer |
| moredata | Integer |
| moreflag | Integer |
| not_counding | Integer |
| number | Integer |
| order | Integer |
| protectedframe | Integer |
| protocol | Integer |
| pwrmgmt | Integer |
| radio | String (the entire [radio header](#the-radio-header)) |
| rate | Integer |
| reason | Integer |
| retry | Integer |
| rssi | Signed Integer |
| rxend_state | Integer |
| sgi | Integer |
| sig_mode | Integer |
| smoothing | Integer |
| srcmac | String |
| srcmac_hex | String |
| status | Integer |
| stbc | Integer |
| subtype | Integer |
| timestamp | String |
| tods | Integer |
| type | Integer |


If you don't know what some of the attributes are, then you probably need to read the IEEE 802.11 specifications and other supporting material.

#### Example
```
print ("SSID", packet.ie_ssid)
```

## The Radio Header

The Radio Header has been mentioned above as a 12 byte structure. The layout is shown below. The only comments are in Chinese.

```
struct {
    signed rssi:8;//表示该包的信号强度
    unsigned rate:4;
    unsigned is_group:1;
    unsigned:1;
    unsigned sig_mode:2;//表示该包是否是11n 的包，0 表示非11n，非0 表示11n
    unsigned legacy_length:12;//如果不是11n 的包，它表示包的长度
    unsigned damatch0:1;
    unsigned damatch1:1;
    unsigned bssidmatch0:1;
    unsigned bssidmatch1:1;
    unsigned MCS:7;//如果是11n 的包，它表示包的调制编码序列，有效值：0-76
    unsigned CWB:1;//如果是11n 的包，它表示是否为HT40 的包
    unsigned HT_length:16;//如果是11n 的包，它表示包的长度
    unsigned Smoothing:1;
    unsigned Not_Sounding:1;
    unsigned:1;
    unsigned Aggregation:1;
    unsigned STBC:2;
    unsigned FEC_CODING:1;//如果是11n 的包，它表示是否为LDPC 的包
    unsigned SGI:1;
    unsigned rxend_state:8;
    unsigned ampdu_cnt:8;
    unsigned channel:4;//表示该包所在的信道
    unsigned:12;
}
```
