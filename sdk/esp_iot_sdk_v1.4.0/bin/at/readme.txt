***********************BOOT MODE***********************
download:
Flash size 8Mbit: 512KB+512KB
boot_v1.2+.bin      	0x00000
user1.1024.new.2.bin  	0x01000
esp_init_data_default.bin	0xfc000	(optional)
blank.bin           	0x7e000 & 0xfe000

Flash size 16Mbit: 512KB+512KB
boot_v1.2+.bin      	0x00000
user1.1024.new.2.bin  	0x01000
esp_init_data_default.bin	0x1fc000 (optional)
blank.bin           	0x7e000 & 0x1fe000

Flash size 16Mbit-C1: 1024KB+1024KB
boot_v1.2+.bin      	0x00000
user1.2048.new.5.bin  	0x01000
esp_init_data_default.bin	0x1fc000 (optional)
blank.bin           	0xfe000 & 0x1fe000

Flash size 32Mbit: 512KB+512KB
boot_v1.2+.bin      	0x00000
user1.1024.new.2.bin  	0x01000
esp_init_data_default.bin	0x3fc000 (optional)
blank.bin           	0x7e000 & 0x3fe000

Flash size 32Mbit-C1: 1024KB+1024KB
boot_v1.2+.bin      	0x00000
user1.2048.new.5.bin  	0x01000
esp_init_data_default.bin	0x3fc000 (optional)
blank.bin           	0xfe000 & 0x3fe000

***********************NON-BOOT MODE***********************
download
eagle.flash.bin		0x00000
eagle.irom0text.bin	0x40000
blank.bin		
	Flash size 4Mbit:	0x3e000	& 0x7e000
	Flash size 8Mbit:	0x7e000 & 0xfe000
	Flash size 16Mbit:      0x7e000 & 0x1fe000
	Flash size 16Mbit-C1:   0xfe000 & 0x1fe000
	Flash size 32Mbit:      0x7e000 & 0x3fe000
	Flash size 32Mbit-C1:   0xfe000 & 0x3fe000
esp_init_data_default.bin	 (optional)	
	Flash size 4Mbit:	0x7c000
      Flash size 8Mbit:	0xfc000
	Flash size 16Mbit:      0x1fc000
	Flash size 16Mbit-C1:   0x1fc000
	Flash size 32Mbit:      0x3fc000
	Flash size 32Mbit-C1:   0x3fc000

*NOTICE*:
UPDATE is not supported in non-boot mode

***********************************************************

Update steps
1.Make sure TE(terminal equipment) is in sta or sta+ap mode
ex. AT+CWMODE=3
    OK
    
    AT+RST

2.Make sure TE got ip address
ex. AT+CWJAP="ssid","12345678"
    OK

    AT+CIFSR
    192.168.1.134

3.Let's update
ex. AT+CIUPDATE
    +CIPUPDATE:1    found server
    +CIPUPDATE:2    connect server
    +CIPUPDATE:3    got edition
    +CIPUPDATE:4    start start

    OK

note. If there are mistakes in the updating, then break update and print ERROR.