#!/usr/bin/python
import os
import sys
import binascii
import string

if len(sys.argv) != 3:
    print 'Usage: gen_flashbin.py 1.bin 2.bin'
    sys.exit(0)

bin1_name = sys.argv[1]
bin2_name = sys.argv[2]

bin1_file = open(bin1_name, 'rb')
bin2_file = open(bin2_name, 'rb')

bin1_len = os.path.getsize(bin1_name)

bin1 = bin1_file.read()
bin2 = bin2_file.read()

bitlist = ['FF']*(0x10000-bin1_len)
bytes = binascii.a2b_hex(''.join(bitlist))

bitout = open('eagle.app.flash.bin', 'wb')
bitout.write(bin1)
bitout.write(bytes)
bitout.write(bin2)
bitout.close()

bin1_file.close()
bin2_file.close()
    
