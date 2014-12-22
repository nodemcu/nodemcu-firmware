#Flash the firmware

eagle.app.v6.flash.bin: 0x00000<br />
eagle.app.v6.irom0text.bin: 0x10000<br />
esp_init_data_default.bin: 0x3fc000<br />
blank.bin: 0x3fe000

###ESP8266's bootloader doesn't support flashing a single file 4Mbytes.
