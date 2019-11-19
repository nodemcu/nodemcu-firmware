local empty = { }
local read_write = {read_only = false}

stds.nodemcu_libs = {
  read_globals = {
    adc = {
      fields = {
        INIT_ADC = empty,
        INIT_VDD33 = empty,
        force_init_mode = empty,
        read = empty,
        readvdd33 = empty
      }
    },
    ads1115 = {
      fields = {
        ADDR_GND = empty,
        ADDR_SCL = empty,
        ADDR_SDA = empty,
        ADDR_VDD = empty,
        CMODE_TRAD = empty,
        CMODE_WINDOW = empty,
        COMP_1CONV = empty,
        COMP_2CONV = empty,
        COMP_4CONV = empty,
        CONTINUOUS = empty,
        CONV_RDY_1 = empty,
        CONV_RDY_2 = empty,
        CONV_RDY_4 = empty,
        DIFF_0_1 = empty,
        DIFF_0_3 = empty,
        DIFF_1_3 = empty,
        DIFF_2_3 = empty,
        DR_128SPS = empty,
        DR_1600SPS = empty,
        DR_16SPS = empty,
        DR_2400SPS = empty,
        DR_250SPS = empty,
        DR_32SPS = empty,
        DR_3300SPS = empty,
        DR_475SPS = empty,
        DR_490SPS = empty,
        DR_64SPS = empty,
        DR_860SPS = empty,
        DR_8SPS = empty,
        DR_920SPS = empty,
        GAIN_0_256V = empty,
        GAIN_0_512V = empty,
        GAIN_1_024V = empty,
        GAIN_2_048V = empty,
        GAIN_4_096V = empty,
        GAIN_6_144V = empty,
        SINGLE_0 = empty,
        SINGLE_1 = empty,
        SINGLE_2 = empty,
        SINGLE_3 = empty,
        SINGLE_SHOT = empty,
        ads1015 = empty,
        ads1115 = empty,
        read = empty,
        reset = empty,
      }
    },
    adxl345 = {
      fields = {
          read = empty,
          setup = empty
      }
    },
    am2320 = {
      fields = {
          read = empty,
          setup = empty
      }
    },
    apa102 = {
      fields = {
        write = empty
      }
    },
    bit = {
       fields = {
         arshift = empty,
         band = empty,
         bit = empty,
         bnot = empty,
         bor = empty,
         bxor = empty,
         clear = empty,
         isclear = empty,
         isset = empty,
         lshift = empty,
         rshift = empty,
         set = empty
      }
    },
    bloom = {
      fields = {
        create = empty
      }
    },
    bme280 = {
      fields = {
        altitude = empty,
        baro = empty,
        dewpoint = empty,
        humi = empty,
        qfe2qnh = empty,
        read = empty,
        setup = empty,
        startreadout = empty,
        temp = empty
      }
    },
    bme680 = {
      fields = {
        altitude = empty,
        dewpoint = empty,
        qfe2qnh = empty,
        read = empty,
        setup = empty,
        startreadout = empty
      }
    },
    bmp085 = {
      fields = {
        pressure = empty,
        pressure_raw = empty,
        setup = empty,
        temperature = empty
      }
    },
    coap = {
      fields = {
        CON = empty,
        Client = empty,
        EXI = empty,
        JSON = empty,
        LINKFORMAT = empty,
        NON = empty,
        OCTET_STREAM = empty,
        Server = empty,
        TEXT_PLAIN = empty,
        XML = empty
      }
    },
    color_utils = {
      fields = {
        colorWheel = empty,
        grb2hsv = empty,
        hsv2grb = empty,
        hsv2grbw = empty
      }
    },
    cron = {
      fields = {
        reset = empty,
        schedule = empty
      }
    },
    crypto = {
      fields = {
        decrypt = empty,
        encrypt = empty,
        fhash = empty,
        hash = empty,
        hmac = empty,
        mask = empty,
        new_hash = empty,
        new_hmac = empty,
        sha1 = empty,
        toBase64 = empty,
        toHex = empty
      }
    },
    dht = {
      fields = {
        ERROR_CHECKSUM = empty,
        ERROR_TIMEOUT = empty,
        OK = empty,
        read = empty,
        read11 = empty,
        readxx = empty
      }
    },
    encoder = {
      fields = {
        fromBase64 = empty,
        fromHex = empty,
        toBase64 = empty,
        toHex = empty
      }
    },
    enduser_setup = {
      fields = {
        manual = empty,
        start = empty,
        stop = empty
      }
    },
    file = {
      fields = {
        close = empty,
        exists = empty,
        flush = empty,
        fsinfo = empty,
        getcontents = empty,
        list = empty,
        on = empty,
        open = empty,
        putcontents = empty,
        read = empty,
        readline = empty,
        remove = empty,
        rename = empty,
        seek = empty,
        stat = empty,
        write = empty,
        writeline = empty
      }
    },
    gdbstub = {
      fields = {
        brk = empty,
        gdboutput = empty,
        open = empty
      }
    },
    gpio = {
      fields = {
        FLOAT = empty,
        HIGH = empty,
        INPUT = empty,
        INT = empty,
        LOW = empty,
        OPENDRAIN = empty,
        OUTPUT = empty,
        PULLUP = empty,
        mode = empty,
        read = empty,
        serout = empty,
        trig = empty,
        write = empty,
        pulse = {
          fields = {
            adjust = empty,
            cancel = empty,
            getstate = empty,
            start = empty,
            stop = empty,
            update = empty
          }
        }
      }
    },
    hdc1080 = {
      fields = {
        read = empty,
        setup = empty
      }
    },
    hmc5883 = {
      fields = {
        read = empty,
        setup = empty
      }
    },
    http = {
      fields = {
        ERROR = empty,
        OK = empty,
        delete = empty,
        get = empty,
        post = empty,
        put = empty,
        request = empty
      }
    },
    hx711 = {
      fields = {
        init = empty,
        read = empty
      }
    },
    i2c = {
      fields = {
        FAST = empty,
        FASTPLUS = empty,
        RECEIVER = empty,
        SLOW = empty,
        TRANSMITTER = empty,
        address = empty,
        read = empty,
        setup = empty,
        start = empty,
        stop = empty,
        write = empty
      }
    },
    l3g4200d = {
      fields = {
        read = empty,
        setup = empty
      }
    },
    mcp4725 = {
      fields = {
        PWRDN_100K = empty,
        PWRDN_1K = empty,
        PWRDN_500K = empty,
        PWRDN_NONE = empty,
        read = empty,
        write = empty
      }
    },
    mdns = {
      fields = {
        close = empty,
        register = empty
      }
    },
    mqtt = {
      fields = {
        CONNACK_ACCEPTED = empty,
        CONNACK_REFUSED_BAD_USER_OR_PASS = empty,
        CONNACK_REFUSED_ID_REJECTED = empty,
        CONNACK_REFUSED_NOT_AUTHORIZED = empty,
        CONNACK_REFUSED_PROTOCOL_VER = empty,
        CONNACK_REFUSED_SERVER_UNAVAILABLE = empty,
        CONN_FAIL_DNS = empty,
        CONN_FAIL_NOT_A_CONNACK_MSG = empty,
        CONN_FAIL_SERVER_NOT_FOUND = empty,
        CONN_FAIL_TIMEOUT_RECEIVING = empty,
        CONN_FAIL_TIMEOUT_SENDING = empty,
        Client = empty
      }
    },
    net = {
      fields = {
        TCP = empty,
        UDP = empty,
        cert = empty,
        createConnection = empty,
        createServer = empty,
        createUDPSocket = empty,
        dns = {
          fields = {
            getdnsserver = empty,
            resolve = empty,
            setdnsserver = empty
          }
        },
        multicastJoin = empty,
        multicastLeave = empty
      }
    },
    node = {
      fields = {
        CPU160MHZ = empty,
        CPU80MHZ = empty,
        bootreason = empty,
        chipid = empty,
        compile = empty,
        dsleep = empty,
        dsleepMax = empty,
        dsleepsetoption = empty,
        flashid = empty,
        flashindex = empty,
        flashreload = empty,
        flashsize = empty,
        getcpufreq = empty,
        getpartitiontable = empty,
        heap = empty,
        info = empty,
        input = empty,
        osprint = empty,
        output = empty,
        random = empty,
        readrcr = empty,
        readvdd33 = empty,
        restart = empty,
        restore = empty,
        setcpufreq = empty,
        setpartitiontable = empty,
        sleep = empty,
        stripdebug = empty,
        writercr = empty,
        egc = {
          fields = {
            setmode = empty,
            meminfo = empty
         }
        },
        task = {
          fields = {
            post = empty
          }
        }
      }
    },
    ow = {
      fields = {
        check_crc16 = empty,
        crc16 = empty,
        crc8 = empty,
        depower = empty,
        read = empty,
        read_bytes = empty,
        reset = empty,
        reset_search = empty,
        search = empty,
        select = empty,
        setup = empty,
        skip = empty,
        target_search = empty,
        write = empty,
        write_bytes = empty
      }
    },
    pcm = {
      fields = {
        RATE_10K = empty,
        RATE_12K = empty,
        RATE_16K = empty,
        RATE_1K = empty,
        RATE_2K = empty,
        RATE_4K = empty,
        RATE_5K = empty,
        RATE_8K = empty,
        SD = empty,
        new = empty
      }
    },
    pwm = {
      fields = {
        close = empty,
        getclock = empty,
        getduty = empty,
        setclock = empty,
        setduty = empty,
        setup = empty,
        start = empty,
        stop = empty
      }
    },
    pwm2 = {
      fields = {
        get_pin_data = empty,
        get_timer_data = empty,
        release_pin = empty,
        set_duty = empty,
        setup_pin_hz = empty,
        setup_pin_sec = empty,
        start = empty,
        stop = empty,
      }
    },
    rc = {
      fields = {
        send = empty
      }
    },
    rfswitch = {
      fields = {
        send = empty
      }
    },
    rotary = {
      fields = {
        ALL = empty,
        CLICK = empty,
        DBLCLICK = empty,
        LONGPRESS = empty,
        PRESS = empty,
        RELEASE = empty,
        TURN = empty,
        close = empty,
        getpos = empty,
        on = empty,
        setup = empty
      }
    },
    rtcfifo = {
      fields = {
        count = empty,
        drop = empty,
        dsleep_until_sample = empty,
        peek = empty,
        pop = empty,
        prepare = empty,
        put = empty,
        ready = empty
      }
    },
    rtcmem = {
      fields = {
        read32 = empty,
        write32 = empty
      }
    },
    rtctime = {
      fields = {
        adjust_delta = empty,
        dsleep = empty,
        dsleep_aligned = empty,
        epoch2cal = empty,
        get = empty,
        set = empty
      }
    },
    si7021 = {
      fields = {
        HEATER_DISABLE = empty,
        HEATER_ENABLE = empty,
        RH08_TEMP12 = empty,
        RH10_TEMP13 = empty,
        RH11_TEMP11 = empty,
        RH12_TEMP14 = empty,
        firmware = empty,
        read = empty,
        serial = empty,
        setting = empty,
        setup = empty
      }
    },
    sigma_delta = {
      fields = {
        close = empty,
        setprescale = empty,
        setpwmduty = empty,
        settarget = empty,
        setup = empty
      }
    },
    sjson = {
      fields = {
        decode = empty,
        decoder = empty,
        encode = empty,
        encoder = empty
      }
    },
    sntp = {
      fields = {
        getoffset = empty,
        setoffset = empty,
        sync = empty
      }
    },
    somfy = {
      fields = {
        DOWN = empty,
        PROG = empty,
        STOP = empty,
        UP = empty,
        sendcommand = empty
      }
    },
    spi = {
      fields = {
        CPHA_HIGH = empty,
        CPHA_LOW = empty,
        CPOL_HIGH = empty,
        CPOL_LOW = empty,
        DATABITS_8 = empty,
        FULLDUPLEX = empty,
        HALFDUPLEX = empty,
        MASTER = empty,
        SLAVE = empty,
        get_miso = empty,
        recv = empty,
        send = empty,
        set_clock_div = empty,
        set_mosi = empty,
        setup = empty,
        transaction = empty
      }
    },
    switec = {
      fields = {
        close = empty,
        dequeue = empty,
        getpos = empty,
        moveto = empty,
        reset = empty,
        setup = empty
      }
    },
    tcs34725 = {
      fields = {
        disable = empty,
        enable = empty,
        raw = empty,
        setGain = empty,
        setIntegrationTime = empty,
        setup = empty
      }
    },
    tls = {
      fields = {
        createConnection = empty,
        setDebug = empty,
        cert = {
          fields = {
            auth = empty,
            verify = empty
          }
        }
      }
    },
    tm1829 = {
      fields = {
        write = empty
      }
    },
    tmr = {
      fields = {
        ALARM_AUTO = empty,
        ALARM_SEMI = empty,
        ALARM_SINGLE = empty,
        create = empty,
        delay = empty,
        now = empty,
        resume_all = empty,
        softwd = empty,
        suspend_all = empty,
        time = empty,
        wdclr = empty
      }
    },
    tsl2561 = {
      fields = {
        ADDRESS_FLOAT = empty,
        ADDRESS_GND = empty,
        ADDRESS_VDD = empty,
        GAIN_16X = empty,
        GAIN_1X = empty,
        INTEGRATIONTIME_101MS = empty,
        INTEGRATIONTIME_13MS = empty,
        INTEGRATIONTIME_402MS = empty,
        PACKAGE_CS = empty,
        PACKAGE_T_FN_CL = empty,
        TSL2561_ERROR_I2CBUSY = empty,
        TSL2561_ERROR_I2CINIT = empty,
        TSL2561_ERROR_LAST = empty,
        TSL2561_ERROR_NOINIT = empty,
        TSL2561_OK = empty,
        getlux = empty,
        getrawchannels = empty,
        init = empty,
        settiming = empty
      }
    },
    -- There would be too many fields for all the fonts and displays
    u8g2 = {other_fields = true},
    uart = {
      fields = {
        PARITY_EVEN = empty,
        PARITY_NONE = empty,
        PARITY_ODD = empty,
        STOPBITS_1 = empty,
        STOPBITS_1_5 = empty,
        STOPBITS_2 = empty,
        alt = empty,
        getconfig = empty,
        on = empty,
        setup = empty,
        write = empty
      }
    },
    -- There would be too many fields for all the fonts and displays
    ucg = {other_fields = true},
    websocket = {
      fields = {
        createClient = empty
      }
    },
    wifi = {
      fields = {
        COUNTRY_AUTO = empty,
        COUNTRY_MANUAL = empty,
        LIGHT_SLEEP = empty,
        MODEM_SLEEP = empty,
        NONE_SLEEP = empty,
        NULLMODE = empty,
        OPEN = empty,
        PHYMODE_B = empty,
        PHYMODE_G = empty,
        PHYMODE_N = empty,
        SOFTAP = empty,
        STATION = empty,
        STATIONAP = empty,
        STA_APNOTFOUND = empty,
        STA_CONNECTING = empty,
        STA_FAIL = empty,
        STA_GOTIP = empty,
        STA_IDLE = empty,
        STA_WRONGPWD = empty,
        WEP = empty,
        WPA2_PSK = empty,
        WPA_PSK = empty,
        WPA_WPA2_PSK = empty,
        getchannel = empty,
        getcountry = empty,
        getdefaultmode = empty,
        getmode = empty,
        getphymode = empty,
        nullmodesleep = empty,
        resume = empty,
        setcountry = empty,
        setmaxtxpower = empty,
        setmode = empty,
        setphymode = empty,
        sleeptype = empty,
        startsmart = empty,
        stopsmart = empty,
        suspend = empty,
        sta = {
          fields = {
            autoconnect = empty,
            changeap = empty,
            clearconfig = empty,
            config = empty,
            connect = empty,
            disconnect = empty,
            getap = empty,
            getapindex = empty,
            getapinfo = empty,
            getbroadcast = empty,
            getconfig = empty,
            getdefaultconfig = empty,
            gethostname = empty,
            getip = empty,
            getmac = empty,
            getrssi = empty,
            setaplimit = empty,
            sethostname = empty,
            setip = empty,
            setmac = empty,
            sleeptype = empty,
            status = empty
          }
        },
        ap = {
          fields = {
            config = empty,
            deauth = empty,
            getbroadcast = empty,
            getclient = empty,
            getconfig = empty,
            getdefaultconfig = empty,
            getip = empty,
            getmac = empty,
            setip = empty,
            setmac = empty,
            dhcp = {
              fields = {
                config = empty,
                start = empty,
                stop = empty
              }
            },
          }
        },
        eventmon = {
          fields = {
            AP_PROBEREQRECVED = empty,
            AP_STACONNECTED = empty,
            AP_STADISCONNECTED = empty,
            EVENT_MAX = empty,
            STA_AUTHMODE_CHANGE = empty,
            STA_CONNECTED = empty,
            STA_DHCP_TIMEOUT = empty,
            STA_DISCONNECTED = empty,
            STA_GOT_IP = empty,
            WIFI_MODE_CHANGED = empty,
            register = empty,
            unregister = empty,
            reason = {
              fields = {
                ["4WAY_HANDSHAKE_TIMEOUT"] = empty,
                ["802_1X_AUTH_FAILED"] = empty,
                AKMP_INVALID = empty,
                ASSOC_EXPIRE = empty,
                ASSOC_FAIL = empty,
                ASSOC_LEAVE = empty,
                ASSOC_NOT_AUTHED = empty,
                ASSOC_TOOMANY = empty,
                AUTH_EXPIRE = empty,
                AUTH_FAIL = empty,
                AUTH_LEAVE = empty,
                BEACON_TIMEOUT = empty,
                CIPHER_SUITE_REJECTED = empty,
                DISASSOC_PWRCAP_BAD = empty,
                DISASSOC_SUPCHAN_BAD = empty,
                GROUP_CIPHER_INVALID = empty,
                GROUP_KEY_UPDATE_TIMEOUT = empty,
                HANDSHAKE_TIMEOUT = empty,
                IE_INVALID = empty,
                IE_IN_4WAY_DIFFERS = empty,
                INVALID_RSN_IE_CAP = empty,
                MIC_FAILURE = empty,
                NOT_ASSOCED = empty,
                NOT_AUTHED = empty,
                NO_AP_FOUND = empty,
                PAIRWISE_CIPHER_INVALID = empty,
                UNSPECIFIED = empty,
                UNSUPP_RSN_IE_VERSION = empty
              }
            }
          }
        },
        monitor = {
          fields = {
            channel = empty,
            start = empty,
            stop = empty
          }
        }
      }
    },
    wps = {
      fields = {
        FAILED = empty,
        SCAN_ERR = empty,
        SUCCESS = empty,
        TIMEOUT = empty,
        WEP = empty,
        disable = empty,
        enable = empty,
        start = empty
      }
    },
    ws2801 = {
      fields = {
        init = empty,
        write = empty
      }
    },
    ws2812 = {
      fields = {
        FADE_IN = empty,
        FADE_OUT = empty,
        MODE_DUAL = empty,
        MODE_SINGLE = empty,
        SHIFT_CIRCULAR = empty,
        SHIFT_LOGICAL = empty,
        init = empty,
        newBuffer = empty,
        write = empty
      }
    },
    ws2812_effects = {
      fields = {
        get_delay = empty,
        get_speed = empty,
        init = empty,
        set_brightness = empty,
        set_color = empty,
        set_delay = empty,
        set_mode = empty,
        set_speed = empty,
        start = empty,
        stop = empty
      }
    },
    xpt2046 = {
      fields = {
        getPosition = empty,
        getPositionAvg = empty,
        getRaw = empty,
        init = empty,
        isTouched = empty,
        setCalibration = empty
      }
    },
    pack = empty,
    unpack  = empty,
    size = empty
  }
}

std = "lua51+nodemcu_libs"