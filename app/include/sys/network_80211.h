#define FRAME_TYPE_MANAGEMENT 0
#define FRAME_TYPE_CONTROL 1
#define FRAME_TYPE_DATA 2
#define FRAME_SUBTYPE_ASSOC_REQUEST 0x00
#define FRAME_SUBTYPE_ASSOC_RESPONSE 0x01
#define FRAME_SUBTYPE_REASSOC_REQUEST 0x02
#define FRAME_SUBTYPE_REASSOC_RESPONSE 0x03
#define FRAME_SUBTYPE_PROBE_REQUEST 0x04
#define FRAME_SUBTYPE_PROBE_RESPONSE 0x05
#define FRAME_SUBTYPE_BEACON 0x08
#define FRAME_SUBTYPE_ATIM 0x09
#define FRAME_SUBTYPE_DISASSOCIATION 0x0a
#define FRAME_SUBTYPE_AUTHENTICATION 0x0b
#define FRAME_SUBTYPE_DEAUTHENTICATION 0x0c
#define FRAME_SUBTYPE_DATA 0x14
typedef struct framectrl_80211
{
    //buf[0]
    u8 Protocol:2;
    u8 Type:2;
    u8 Subtype:4;
    //buf[1]
    u8 ToDS:1;
    u8 FromDS:1;
    u8 MoreFlag:1;
    u8 Retry:1;
    u8 PwrMgmt:1;
    u8 MoreData:1;
    u8 Protectedframe:1;
    u8 Order:1;
} framectrl_80211,*lpframectrl_80211;

typedef struct management_80211
{
	struct framectrl_80211 framectrl;
	uint16 duration;
	uint8 rdaddr[6];
	uint8 tsaddr[6];
	uint8 bssid[6];
	uint16 number;
} management_request_t;

typedef struct
{
	management_request_t hdr;
        uint8 timestamp[8];
	uint16 beacon_interval;
	uint16 capability_info;
} wifi_beacon_t;

typedef struct tagged_parameter
{
	/* SSID parameter */
	uint8 tag_number;
	uint8 tag_length;
} tagged_parameter, *ptagged_parameter;

struct RxControl {
    signed rssi:8;//表示该包的信号强度
    unsigned rate:4;
    unsigned is_group:1;
    unsigned:1;
    unsigned sig_mode:2;//表示该包是否是11n的包，0表示非11n，非0表示11n
    unsigned legacy_length:12;//如果不是11n的包，它表示包的长度
    unsigned damatch0:1;
    unsigned damatch1:1;
    unsigned bssidmatch0:1;
    unsigned bssidmatch1:1;
    unsigned MCS:7;//如果是11n的包，它表示包的调制编码序列，有效值：0-76
    unsigned CWB:1;//如果是11n的包，它表示是否为HT40的包
    unsigned HT_length:16;//如果是11n的包，它表示包的长度
    unsigned Smoothing:1;
    unsigned Not_Sounding:1;
    unsigned:1;
    unsigned Aggregation:1;
    unsigned STBC:2;
    unsigned FEC_CODING:1;//如果是11n的包，它表示是否为LDPC的包
    unsigned SGI:1;
    unsigned rxend_state:8;
    unsigned ampdu_cnt:8;
    unsigned channel:4;//表示该包所在的信道
    unsigned:12;
};

struct sniffer_buf2{
	struct RxControl rx_ctrl;
	u8 buf[112];//包含ieee80211包头
	u16 cnt;//包的个数
	u16 len[1];//包的长度
};

struct sniffer_buf{
	struct RxControl rx_ctrl;
	u8 buf[48];//包含ieee80211包头
	u16 cnt;//包的个数
	u16 len[1];//包的长度
};

