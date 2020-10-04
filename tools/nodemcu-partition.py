#!/usr/bin/env python
#
# ESP8266 LFS Loader Utility
#
# Copyright (C) 2019 Terry Ellison, NodeMCU Firmware Community Project. drawing
# heavily from and including content from esptool.py with full acknowledgement
# under GPL 2.0, with said content: Copyright (C) 2014-2016 Fredrik Ahlberg, Angus
# Gratton, Espressif Systems  (Shanghai) PTE LTD, other contributors as noted.
# https:# github.com/espressif/esptool
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

import os
import sys
print os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/toolchains/')
import esptool

import io
import tempfile
import shutil

from pprint import pprint

import argparse
import gzip
import copy
import inspect
import struct
import string
import math

__version__     = '1.0'
__program__     = 'nodemcu-partition.py'
ROM0_Seg        =   0x010000
FLASH_PAGESIZE  =   0x001000
FLASH_BASE_ADDR = 0x40200000
PARTITION_TYPE  = {
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

SYSTEM_PARAMETER = 6
IROM0TEXT        = 102
LFS              = 103
SPIFFS           = 106

MAX_PT_SIZE = 20*3
FLASH_SIG          = 0xfafaa150
FLASH_SIG_MASK     = 0xfffffff0
FLASH_SIG_ABSOLUTE = 0x00000001
WORDSIZE           = 4
WORDBITS           = 32

DEFAULT_FLASH_SIZE   = 4*1024*1024
PLATFORM_RCR_DELETED =   0x0
PLATFORM_RCR_PT      =   0x1
PLATFORM_RCR_FREE    =  0xFF

SPIFFS_USE_ALL      = 0xFFFFFFFF

PACK_INT = struct.Struct("<I")

class FatalError(RuntimeError):
    def __init__(self, message):
        RuntimeError.__init__(self, message)

    def WithResult(message, result):
        message += " (result was %s)" % hexify(result)
        return FatalError(message)

def alignPT(n):
    return 2*FLASH_PAGESIZE*int(math.ceil(n/2/FLASH_PAGESIZE))

def unpack_RCR(data):
    RCRword,recs, i = [PACK_INT.unpack_from(data,i)[0] \
                          for i in range(0, FLASH_PAGESIZE, WORDSIZE)], \
                      [],0
    while RCRword[i] % 256 != PLATFORM_RCR_FREE:
        Rlen, Rtype = RCRword[i] % 256, (RCRword[i]/256) % 256
        if Rtype != PLATFORM_RCR_DELETED:
            rec = [Rtype,[RCRword[j] for j in range(i+1,i+1+Rlen)]]
            if Rtype == PLATFORM_RCR_PT:
                PTrec = rec[1]
            else:
                recs.append(rec)
        i = i + Rlen + 1

    if PTrec is not None:
        return PTrec,recs

    FatalError("No partition table found")

def repack_RCR(recs):
    data = []
    for r in recs:
        Rtype, Rdata = r
        data.append(256*Rtype + len(Rdata))
        data.extend(Rdata)

    return ''.join([PACK_INT.pack(i) for i in data])

def load_PT(data, args):
    """
    Load the Flash copy of the Partition Table from the first segment of the IROM0
    segment, that is at 0x10000.  If nececessary the LFS partition is then correctly
    positioned and adjusted according to the optional start and len arguments.

    The (possibly) updated PT is then returned with the LFS sizing.
    """

    PTrec,recs = unpack_RCR(data)
    flash_size = args.fs if args.fs is not None else DEFAULT_FLASH_SIZE

    # The partition table format is a set of 3*uint32 fields (type, addr, size),
    # with the optional last slot being an end marker (0,size,0) where size is
    # of the firmware image.

    if PTrec[-3] == 0:             # Pick out the ROM size and remove the marker
        defaultIROM0size = PTrec[-2] - FLASH_BASE_ADDR
        del PTrec[-3:]
    else:
        defaultIROM0size = None

    # The SDK objects to zero-length partitions so if the developer sets the
    # size of the LFS and/or the SPIFFS partition to 0 then this is removed.
    # If it is subsequently set back to non-zero then it needs to be reinserted.
    # In reality the sizing algos assume that the LFS follows the IROM0TEXT one
    # and SPIFFS is the last partition.  We will need to revisit these algos if
    # we adopt a more flexible partiton allocation policy.  *** BOTCH WARNING ***

    i = 0
    while i < len(PTrec):
        if PTrec[i] == IROM0TEXT and args.ls is not None and \
            (len(PTrec) == i+3  or PTrec[i+3] != LFS):
            PTrec[i+3:i+3] = [LFS, 0, 0]
        i += 3
        
    if PTrec[-6] != SPIFFS:
        PTrec[-6:-6] = [SPIFFS, PTrec[-5] + PTrec[-4], 0x1000]

    lastEnd, newPT, map = 0,[], dict()
    print "  Partition          Start   Size \n  ------------------ ------ ------"
    for i in range (0, len(PTrec), 3):
        Ptype, Paddr, Psize = PTrec[i:i+3]

        if Ptype == IROM0TEXT:
            # If the IROM0 partition size is 0 then compute from the IROM0_SIZE.
            # Note that this script uses the size in the end-marker as a default
            if Psize == 0:
                if defaultIROM0size is None:
                    raise FatalError("Cannot set the IROM0 partition size")
                Psize = alignPT(defaultIROM0size)

        elif Ptype == LFS:
            #  Properly align the LFS partition size and make it consecutive to
            #  the previous partition.
            if args.la is not None:
                Paddr = args.la
            if args.ls is not None:
                Psize = args.ls
            Psize = alignPT(Psize)
            if Paddr == 0:
                Paddr = lastEnd
            if Psize > 0:
                map['LFS'] = {"addr" : Paddr, "size" : Psize}

        elif Ptype == SPIFFS:
            # The logic here is convolved.  Explicit start and length can be
            # set, but the SPIFFS region is aslo contrained by the end of the
            # previos partition and the end of Flash.  The size = -1 value
            # means use up remaining flash and the SPIFFS will be moved to the
            # 1Mb boundary if the address is default and the specified size
            # allows this.
            if args.sa is not None:
                Paddr = args.sa
            if args.ss is not None:
                Psize = args.ss if args.ss >= 0 else SPIFFS_USE_ALL
            if Psize == SPIFFS_USE_ALL:
                #  This allocate all the remaining flash to SPIFFS
                if Paddr < lastEnd:
                    Paddr = lastEnd
                Psize = flash_size - Paddr
            else:
                if Paddr == 0:
                    #  if the is addr not specified then start SPIFFS at 1Mb
                    #  boundary if the size will fit otherwise make it consecutive
                    #  to the previous partition.
                    Paddr = 0x100000  if Psize <= flash_size - 0x100000 else lastEnd
                elif Paddr < lastEnd:
                    Paddr = lastEnd
                if Psize > flash_size - Paddr:
                    Psize = flash_size - Paddr
            if Psize > 0:
                map['SPIFFS'] = {"addr" : Paddr, "size" : Psize}

        elif Ptype == SYSTEM_PARAMETER and Paddr == 0:
            Paddr = flash_size - Psize
 
        if Psize > 0:
            Pname = PARTITION_TYPE[Ptype] if Ptype in PARTITION_TYPE \
                                          else ("Type %d" % Ptype)
            print("  %-18s %06x %06x"% (Pname, Paddr, Psize))
           #  Do consistency tests on the partition
            if (Paddr & (FLASH_PAGESIZE - 1)) > 0 or \
               (Psize & (FLASH_PAGESIZE - 1)) > 0 or \
                Paddr < lastEnd or \
                Paddr + Psize > flash_size:
                print (lastEnd, flash_size)
                raise FatalError("Partition %u invalid alignment\n" % (i/3))

            newPT.extend([Ptype, Paddr, Psize])
            lastEnd = Paddr + Psize

    recs.append([PLATFORM_RCR_PT,newPT])
    return recs, map

def relocate_lfs(data, addr, size):
    """
    The unpacked LFS image comprises the relocatable image itself, followed by a bit
    map (one bit per word) flagging if the corresponding word of the image needs
    relocating.  The image and bitmap are enumerated with any addresses being
    relocated by the LFS base address.  (Note that the PIC format of addresses is word
    aligned and so first needs scaling by the wordsize.)
    """
    addr += FLASH_BASE_ADDR
    w = [PACK_INT.unpack_from(data,i)[0] for i in range(0, len(data),WORDSIZE)]
    flash_sig, flash_size = w[0], w[1]

    assert ((flash_sig & FLASH_SIG_MASK) == FLASH_SIG and
            (flash_sig & FLASH_SIG_ABSOLUTE) == 0 and
             flash_size % WORDSIZE == 0)

    flash_size //= WORDSIZE
    flags_size = (flash_size + WORDBITS - 1) // WORDBITS

    print WORDSIZE*flash_size, size, len(data), WORDSIZE*(flash_size + flags_size)
    assert (WORDSIZE*flash_size <= size and
            len(data) == WORDSIZE*(flash_size + flags_size))

    image,flags,j    = w[0:flash_size], w[flash_size:], 0

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
        ux = x.upper()
        if "M" in ux:
            return int(ux[:ux.index("M")]) * 1024 * 1024
        elif "K" in ux:
            return int(ux[:ux.index("K")]) * 1024
        else:
            return int(ux, 0)

    print('%s V%s' %(__program__, __version__))

    # ---------- process the arguments ---------- #

    a = argparse.ArgumentParser(
        description='%s V%s - ESP8266 NodeMCU Loader Utility' %
                     (__program__, __version__),
        prog=__program__)
    a.add_argument('--port', '-p', help='Serial port device')
    a.add_argument('--baud', '-b',  type=arg_auto_int,
        help='Serial port baud rate used when flashing/reading')
    a.add_argument('--flash_size', '-fs', dest="fs", type=arg_auto_int,
        help='Flash size used in SPIFFS allocation (Default 4MB)')
    a.add_argument('--lfs_addr', '-la', dest="la", type=arg_auto_int,
        help='(Overwrite) start address of LFS partition')
    a.add_argument('--lfs_size', '-ls', dest="ls", type=arg_auto_int,
        help='(Overwrite) length of LFS partition')
    a.add_argument('--lfs_file', '-lf', dest="lf", help='LFS image file')
    a.add_argument('--spiffs_addr', '-sa', dest="sa", type=arg_auto_int,
        help='(Overwrite) start address of SPIFFS partition')
    a.add_argument('--spiffs_size', '-ss', dest="ss", type=arg_auto_int,
        help='(Overwrite) length of SPIFFS partition')
    a.add_argument('--spiffs_file', '-sf', dest="sf", help='SPIFFS image file')

    arg = a.parse_args()

    if arg.lf is not None:
        if not os.path.exists(arg.lf):
            raise FatalError("LFS image %s does not exist" % arg.lf)

    if arg.sf is not None:
        if not os.path.exists(arg.sf):
           raise FatalError("SPIFFS image %s does not exist" % arg.sf)

    base = [] if arg.port is None else ['--port',arg.port]
    if arg.baud is not None: base.extend(['--baud',str(arg.baud)])

    # ---------- Use esptool to read the PT ---------- #

    tmpdir  = tempfile.mkdtemp()
    pt_file = tmpdir + '/pt.dmp'
    espargs = base+['--after', 'no_reset', 'read_flash', '--no-progress',
                    str(ROM0_Seg), str(FLASH_PAGESIZE), pt_file]
                    
    esptool.main(espargs)

    with open(pt_file,"rb") as f:
        data = f.read()

    # ---------- Update the PT if necessary ---------- #

    recs, pt_map = load_PT(data, arg)
    odata = repack_RCR(recs)
    odata = odata + "\xFF" * (FLASH_PAGESIZE - len(odata))

    # ---------- If the PT has changed then use esptool to rewrite it ---------- #

    if odata != data:
        print("PT updated")
        pt_file = tmpdir + '/opt.dmp'
        with open(pt_file,"wb") as f:
            f.write(odata)
        espargs = base+['--after', 'no_reset', 'write_flash', '--no-progress',
                        str(ROM0_Seg), pt_file]
        esptool.main(espargs)

    if arg.lf is not None:
        if 'LFS' not in pt_map:
            raise FatalError("No LFS partition; cannot write LFS image")
        la,ls = pt_map['LFS']['addr'], pt_map['LFS']['size']

        # ---------- Read and relocate the LFS image ---------- #

        with gzip.open(arg.lf) as f:
            lfs = f.read()
            if len(lfs) > ls:
                raise FatalError("LFS partition to small for LFS image")
            lfs = relocate_lfs(lfs, la, ls)

        # ---------- Write to a temp file and use esptool to write it to flash ---------- #

        img_file = tmpdir + '/lfs.img'
        espargs = base + ['write_flash', str(la), img_file]
        with open(img_file,"wb") as f:
            f.write(lfs)
        esptool.main(espargs)

    if arg.sf is not None:
        if 'SPIFFS' not in pt_map:
            raise FatalError("No SPIFSS partition; cannot write SPIFFS image")
        sa,ss = pt_map['SPIFFS']['addr'], pt_map['SPIFFS']['size']

        # ---------- Write to a temp file and use esptool to write it to flash ---------- #

        spiffs_file = arg.sf
        espargs = base + ['write_flash', str(sa), spiffs_file]
        esptool.main(espargs)

    # ---------- Clean up temp directory ---------- #

#    espargs = base + ['--after', 'hard_reset', 'flash_id']
#    esptool.main(espargs)

    shutil.rmtree(tmpdir)

def _main():
        main()

if __name__ == '__main__':
    _main()
