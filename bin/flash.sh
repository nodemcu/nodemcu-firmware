esptool.py --port /dev/ttyUSB0 write_flash 0x00000 eagle.app.v6.flash.bin 0x10000 eagle.app.v6.irom0text.bin 0x7c000 blank.bin 0x7e000 esp_init_data_default.bin
