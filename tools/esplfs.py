#!/usr/bin/env python
#
# ESP8266 LFS Loader Utility
#
# Copyright (C) 2019 Terry Ellison, NodeMCU Firmware Community Project. drawing 
# heavily from and including content from esptool.py with full acknowledgement 
# under GPL 2.0, with said content: Copyright (C) 2014-2016 Fredrik Ahlberg, Angus 
# Gratton, Espressif Systems  (Shanghai) PTE LTD, other contributors as noted. 
# https://github.com/espressif/esptool
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301 USA.

import io
import os
import sys
sys.path.append(os.path.realpath(os.path.dirname(__file__) + '/toolchains/'))
import esptool

import tempfile
import shutil

from pprint import pprint

from esptool import ESPLoader, ESP8266ROM
import argparse
import gzip
import base64
import binascii
import copy
import hashlib
import inspect
import shlex
import struct
import time
import string


MAX_UINT32 = 0xffffffff
MAX_UINT24 = 0xffffff

ROM0_Seg        =   0x010000
FLASH_PAGESIZE  =   0x001000
FLASH_BASE_ADDR = 0x40200000
__version__     = '1.0'

PARTITION_TYPES = {
       4: 'RF_CAL',
       5: 'PHY_DATA',
       6: 'SYSTEM_PARAMETER',
     101: 'EAGLEROM',
     102: 'IROM0TEXT',
     103: 'LFS0',
     104: 'LFS1',
     105: 'TLSCERT',
     106: 'SPIFFS0',
     107: 'SPIFFS1'}
 
MAX_PT_SIZE = 20*3
PACK_INT    = struct.Struct("<I")

def load_PT(data, args):
    print('data len= %u' % len(data))
    pt = [PACK_INT.unpack_from(data,4*i)[0] for i in range(0, MAX_PT_SIZE)]

    n, flash_used_end, rewrite = 0, 0, False
    LFSaddr, LFSsize = None, None

    pt_map = dict()
    for i in range(0,60,3):
        pType = pt[i];
        if pType == 0:
            n = i // 3
            flash_used_end = pt[i+1]
            break
        else:
            pt_map[PARTITION_TYPES[pType]] = i

    if not ('IROM0TEXT' in pt_map and 'LFS0' in pt_map):
        raise FatalError("Partition table must contain IROM0 and LFS segments")

    i = pt_map['IROM0TEXT']
    if pt[i+2] == 0:
        pt[i+2] = (flash_used_end - FLASH_BASE_ADDR) - pt[i+1]
        rewrite = True

    j = pt_map['LFS0']
    if args.start is not None and pt[j+1] != args.start:
        pt[j+1] = args.start
    elif pt[j+1] == 0:
        pt[j+1] = pt[i+1] + pt[i+2]
 
    if args.len is not None and pt[j+2] != args.len:
        pt[j+2] = args.len
 
    LFSaddr, LFSsize = pt[j+1], pt[j+2]
    print ('\nDump of Partition Table\n')

    for i in range(0,3*n,3):
        print ('%-18s  0x%06x  0x%06x' % (PARTITION_TYPES[pt[i]], pt[i+1], pt[i+2]))

    odata = ''.join([PACK_INT.pack(pt[i]) for i in range(0,3*n)]) + data[3*4*n:]

    return odata, LFSaddr, LFSsize


FLASH_SIG          = 0xfafaa150
FLASH_SIG_MASK     = 0xfffffff0
FLASH_SIG_ABSOLUTE = 0x00000001
WORDSIZE           = 4
WORDBITS           = 32

def relocate_lfs(data, addr, size):
    addr += FLASH_BASE_ADDR
    w = [PACK_INT.unpack_from(data,WORDSIZE*i)[0] for i in range(0, len(data) // WORDSIZE)]
    flash_sig, flash_size = w[0], w[1]
    flags_size = 1 + (flash_size - 1) // WORDBITS
    print('%08x'%flash_sig)
    assert ((flash_sig & FLASH_SIG_MASK) == FLASH_SIG and 
            (flash_sig & FLASH_SIG_ABSOLUTE) == 0 and
            flash_size <= size and 
            flash_size % WORDSIZE == 0 and
            len(data) == flash_size + flags_size)

    flags,j = w[flash_size // WORDSIZE:], 0
    image = w[0:flash_size // WORDSIZE]

    for i in range(0,len(image)):
        if i % WORDBITS == 0:
            flag_word = flags[j]
            j += 1
        if (flag_word & 1) == 1:
            o = image[i]
            image[i] = WORDSIZE*image[i] + addr
        flag_word >>= 1

    return ''.join([PACK_INT.pack(i) for i in image])

def main():

    def arg_auto_int(x):
        return int(x, 0)

    print('esplfs v%s' % __version__)

    a = argparse.ArgumentParser(
        description='esplfs.py v%s - ESP8266 NodeMCU LFS Loader Utility' % __version__, 
        prog='esplfs')
    a.add_argument('--port', '-p', help='Serial port device')
    a.add_argument('--baud', '-b',  type=arg_auto_int,
        help='Serial port baud rate used when flashing/reading')
    a.add_argument('--start', '-s', type=arg_auto_int, 
        help='(Overwrite) Start of LFS partition')
    a.add_argument('--len', '-l',  type=arg_auto_int, 
        help='(Overwrite) length of LFS partition')
    a.add_argument('filename', help='filename of LFS image', metavar='LFSimg')

    arg = a.parse_args()

    base = [] if arg.port is None else ['--port',arg.port]
    if arg.baud is not None: base.extend(['--baud',arg.baud])
    
    tmpdir = tempfile.mkdtemp()
    pt_file = tmpdir + '/pt.dmp'
    espargs = base+['--after', 'no_reset', 'read_flash', '--no-progress',
                    str(ROM0_Seg), str(FLASH_PAGESIZE), pt_file]
    esptool.main(espargs)

    with open(pt_file,"rb") as f:
        data = f.read()
    odata, LFSaddr, LFSsize = load_PT(data, arg)
    
    if odata != data:
        print("PT updated")
        pt_file = tmpdir + '/opt.dmp'
        with open(pt_file,"wb") as f:
            f.write(odata)
        espargs = base+['--after', 'no_reset', 'write_flash', '--no-progress',
                        str(ROM0_Seg), pt_file]
        esptool.main(espargs)
        sys.exit()

    with gzip.open(arg.filename) as f:
        lfs = f.read()

    print('LFS len = %u' % len(lfs)) 
    lfs = relocate_lfs(lfs, LFSaddr, LFSsize)

    img_file = tmpdir + '/lfs.img'
    espargs = base + ['write_flash', str(LFSaddr), img_file]
    with open(img_file,"wb") as f:
        f.write(lfs)
    esptool.main(espargs)

#    shutil.rmtree(tmpdir)
 
def _main():
##    try:
        main()
##    except:
##        print('\nA fatal error occurred')
##        sys.exit(2)


if __name__ == '__main__':
    _main()
