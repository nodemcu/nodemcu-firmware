#!/usr/bin/env python
#
# ESP8266 ROM Bootloader Utility
# https://github.com/themadinventor/esptool
#
# Copyright (C) 2014 Fredrik Ahlberg
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

import sys
import struct
import serial
import math
import time
import argparse
import os
import subprocess

class ESPROM:

    # These are the currently known commands supported by the ROM
    ESP_FLASH_BEGIN = 0x02
    ESP_FLASH_DATA  = 0x03
    ESP_FLASH_END   = 0x04
    ESP_MEM_BEGIN   = 0x05
    ESP_MEM_END     = 0x06
    ESP_MEM_DATA    = 0x07
    ESP_SYNC        = 0x08
    ESP_WRITE_REG   = 0x09
    ESP_READ_REG    = 0x0a

    # Maximum block sized for RAM and Flash writes, respectively.
    ESP_RAM_BLOCK   = 0x1800
    ESP_FLASH_BLOCK = 0x400

    # Default baudrate. The ROM auto-bauds, so we can use more or less whatever we want.
    ESP_ROM_BAUD    = 115200

    # First byte of the application image
    ESP_IMAGE_MAGIC = 0xe9

    # Initial state for the checksum routine
    ESP_CHECKSUM_MAGIC = 0xef

    # OTP ROM addresses
    ESP_OTP_MAC0    = 0x3ff00050
    ESP_OTP_MAC1    = 0x3ff00054

    # Sflash stub: an assembly routine to read from spi flash and send to host
    SFLASH_STUB     = "\x80\x3c\x00\x40\x1c\x4b\x00\x40\x21\x11\x00\x40\x00\x80" \
            "\xfe\x3f\xc1\xfb\xff\xd1\xf8\xff\x2d\x0d\x31\xfd\xff\x41\xf7\xff\x4a" \
            "\xdd\x51\xf9\xff\xc0\x05\x00\x21\xf9\xff\x31\xf3\xff\x41\xf5\xff\xc0" \
            "\x04\x00\x0b\xcc\x56\xec\xfd\x06\xff\xff\x00\x00"

    def __init__(self, port = 0, baud = ESP_ROM_BAUD):
        self._port = serial.Serial(port, baud)

    """ Read bytes from the serial port while performing SLIP unescaping """
    def read(self, length = 1):
        b = ''
        while len(b) < length:
            c = self._port.read(1)
            if c == '\xdb':
                c = self._port.read(1)
                if c == '\xdc':
                    b = b + '\xc0'
                elif c == '\xdd':
                    b = b + '\xdb'
                else:
                    raise Exception('Invalid SLIP escape')
            else:
                b = b + c
        return b

    """ Write bytes to the serial port while performing SLIP escaping """
    def write(self, packet):
        buf = '\xc0'+(packet.replace('\xdb','\xdb\xdd').replace('\xc0','\xdb\xdc'))+'\xc0'
        self._port.write(buf)

    """ Calculate checksum of a blob, as it is defined by the ROM """
    @staticmethod
    def checksum(data, state = ESP_CHECKSUM_MAGIC):
        for b in data:
            state ^= ord(b)
        return state

    """ Send a request and read the response """
    def command(self, op = None, data = None, chk = 0):
        if op:
            # Construct and send request
            pkt = struct.pack('<BBHI', 0x00, op, len(data), chk) + data
            self.write(pkt)

        # Read header of response and parse
        if self._port.read(1) != '\xc0':
            raise Exception('Invalid head of packet')
        hdr = self.read(8)
        (resp, op_ret, len_ret, val) = struct.unpack('<BBHI', hdr)
        if resp != 0x01 or (op and op_ret != op):
            raise Exception('Invalid response')

        # The variable-length body
        body = self.read(len_ret)

        # Terminating byte
        if self._port.read(1) != chr(0xc0):
            raise Exception('Invalid end of packet')

        return val, body

    """ Perform a connection test """
    def sync(self):
        self.command(ESPROM.ESP_SYNC, '\x07\x07\x12\x20'+32*'\x55')
        for i in xrange(7):
            self.command()

    """ Try connecting repeatedly until successful, or giving up """
    def connect(self):
        print 'Connecting...'

        for _ in xrange(4):
            # issue reset-to-bootloader:
            # RTS = either CH_PD or nRESET (both active low = chip in reset)
            # DTR = GPIO0 (active low = boot to flasher)
            self._port.setDTR(False)
            self._port.setRTS(True)
            time.sleep(0.05)
            self._port.setDTR(True)
            self._port.setRTS(False)
            time.sleep(0.05)
            self._port.setDTR(False)

            self._port.timeout = 0.3 # worst-case latency timer should be 255ms (probably <20ms)
            for _ in xrange(4):
                try:
                    self._port.flushInput()
                    self._port.flushOutput()
                    self.sync()
                    self._port.timeout = 5
                    return
                except:
                    time.sleep(0.05)
            # this is a workaround for the CH340 serial driver on current versions of Linux,
            # which seems to sometimes set the serial port up with wrong parameters
            self._port.close()
            self._port.open()
        raise Exception('Failed to connect')

    """ Read memory address in target """
    def read_reg(self, addr):
        res = self.command(ESPROM.ESP_READ_REG, struct.pack('<I', addr))
        if res[1] != "\0\0":
            raise Exception('Failed to read target memory')
        return res[0]

    """ Write to memory address in target """
    def write_reg(self, addr, value, mask, delay_us = 0):
        if self.command(ESPROM.ESP_WRITE_REG,
                struct.pack('<IIII', addr, value, mask, delay_us))[1] != "\0\0":
            raise Exception('Failed to write target memory')

    """ Start downloading an application image to RAM """
    def mem_begin(self, size, blocks, blocksize, offset):
        if self.command(ESPROM.ESP_MEM_BEGIN,
                struct.pack('<IIII', size, blocks, blocksize, offset))[1] != "\0\0":
            raise Exception('Failed to enter RAM download mode')

    """ Send a block of an image to RAM """
    def mem_block(self, data, seq):
        if self.command(ESPROM.ESP_MEM_DATA,
                struct.pack('<IIII', len(data), seq, 0, 0)+data, ESPROM.checksum(data))[1] != "\0\0":
            raise Exception('Failed to write to target RAM')

    """ Leave download mode and run the application """
    def mem_finish(self, entrypoint = 0):
        if self.command(ESPROM.ESP_MEM_END,
                struct.pack('<II', int(entrypoint == 0), entrypoint))[1] != "\0\0":
            raise Exception('Failed to leave RAM download mode')

    """ Start downloading to Flash (performs an erase) """
    def flash_begin(self, size, offset):
        old_tmo = self._port.timeout
        num_blocks = (size + ESPROM.ESP_FLASH_BLOCK - 1) / ESPROM.ESP_FLASH_BLOCK
        self._port.timeout = 10
        if self.command(ESPROM.ESP_FLASH_BEGIN,
                struct.pack('<IIII', size, num_blocks, ESPROM.ESP_FLASH_BLOCK, offset))[1] != "\0\0":
            raise Exception('Failed to enter Flash download mode')
        self._port.timeout = old_tmo

    """ Write block to flash """
    def flash_block(self, data, seq):
        if self.command(ESPROM.ESP_FLASH_DATA,
                struct.pack('<IIII', len(data), seq, 0, 0)+data, ESPROM.checksum(data))[1] != "\0\0":
            raise Exception('Failed to write to target Flash')

    """ Leave flash mode and run/reboot """
    def flash_finish(self, reboot = False):
        pkt = struct.pack('<I', int(not reboot))
        if self.command(ESPROM.ESP_FLASH_END, pkt)[1] != "\0\0":
            raise Exception('Failed to leave Flash mode')

    """ Run application code in flash """
    def run(self, reboot = False):
        # Fake flash begin immediately followed by flash end
        self.flash_begin(0, 0)
        self.flash_finish(reboot)

    """ Read MAC from OTP ROM """
    def read_mac(self):
        mac0 = esp.read_reg(esp.ESP_OTP_MAC0)
        mac1 = esp.read_reg(esp.ESP_OTP_MAC1)
        if ((mac1 >> 16) & 0xff) == 0:
            oui = (0x18, 0xfe, 0x34)
        elif ((mac1 >> 16) & 0xff) == 1:
            oui = (0xac, 0xd0, 0x74)
        else:
            raise Exception("Unknown OUI")
        return oui + ((mac1 >> 8) & 0xff, mac1 & 0xff, (mac0 >> 24) & 0xff)

    """ Read SPI flash manufacturer and device id """
    def flash_id(self):
        self.flash_begin(0, 0)
        self.write_reg(0x60000240, 0x0, 0xffffffff)
        self.write_reg(0x60000200, 0x10000000, 0xffffffff)
        flash_id = esp.read_reg(0x60000240)
        self.flash_finish(False)
        return flash_id

    """ Read SPI flash """
    def flash_read(self, offset, size, count = 1):
        # Create a custom stub
        stub = struct.pack('<III', offset, size, count) + self.SFLASH_STUB

        # Trick ROM to initialize SFlash
        self.flash_begin(0, 0)

        # Download stub
        self.mem_begin(len(stub), 1, len(stub), 0x40100000)
        self.mem_block(stub, 0)
        self.mem_finish(0x4010001c)

        # Fetch the data
        data = ''
        for _ in xrange(count):
            if self._port.read(1) != '\xc0':
                raise Exception('Invalid head of packet (sflash read)')

            data += self.read(size)

            if self._port.read(1) != chr(0xc0):
                raise Exception('Invalid end of packet (sflash read)')

        return data

    """ Abuse the loader protocol to force flash to be left in write mode """
    def flash_unlock_dio(self):
        # Enable flash write mode
        self.flash_begin(0, 0)
        # Reset the chip rather than call flash_finish(), which would have
        # write protected the chip again (why oh why does it do that?!)
        self.mem_begin(0,0,0,0x40100000)
        self.mem_finish(0x40000080)

    """ Perform a chip erase of SPI flash """
    def flash_erase(self):
        # Trick ROM to initialize SFlash
        self.flash_begin(0, 0)

        # This is hacky: we don't have a custom stub, instead we trick
        # the bootloader to jump to the SPIEraseChip() routine and then halt/crash
        # when it tries to boot an unconfigured system.
        self.mem_begin(0,0,0,0x40100000)
        self.mem_finish(0x40004984)

        # Yup - there's no good way to detect if we succeeded.
        # It it on the other hand unlikely to fail.

class ESPFirmwareImage:
    
    def __init__(self, filename = None):
        self.segments = []
        self.entrypoint = 0
        self.flash_mode = 0
        self.flash_size_freq = 0

        if filename is not None:
            f = file(filename, 'rb')
            (magic, segments, self.flash_mode, self.flash_size_freq, self.entrypoint) = struct.unpack('<BBBBI', f.read(8))
            
            # some sanity check
            if magic != ESPROM.ESP_IMAGE_MAGIC or segments > 16:
                raise Exception('Invalid firmware image')
        
            for i in xrange(segments):
                (offset, size) = struct.unpack('<II', f.read(8))
                if offset > 0x40200000 or offset < 0x3ffe0000 or size > 65536:
                    raise Exception('Suspicious segment %x,%d' % (offset, size))
                self.segments.append((offset, size, f.read(size)))

            # Skip the padding. The checksum is stored in the last byte so that the
            # file is a multiple of 16 bytes.
            align = 15-(f.tell() % 16)
            f.seek(align, 1)

            self.checksum = ord(f.read(1))

    def add_segment(self, addr, data):
        # Data should be aligned on word boundary
        l = len(data)
        if l % 4:
            data += b"\x00" * (4 - l % 4)
        self.segments.append((addr, len(data), data))

    def save(self, filename):
        f = file(filename, 'wb')
        f.write(struct.pack('<BBBBI', ESPROM.ESP_IMAGE_MAGIC, len(self.segments),
            self.flash_mode, self.flash_size_freq, self.entrypoint))

        checksum = ESPROM.ESP_CHECKSUM_MAGIC
        for (offset, size, data) in self.segments:
            f.write(struct.pack('<II', offset, size))
            f.write(data)
            checksum = ESPROM.checksum(data, checksum)

        align = 15-(f.tell() % 16)
        f.seek(align, 1)
        f.write(struct.pack('B', checksum))


class ELFFile:

    def __init__(self, name):
        self.name = name
        self.symbols = None

    def _fetch_symbols(self):
        if self.symbols is not None:
            return
        self.symbols = {}
        try:
            tool_nm = "xtensa-lx106-elf-nm"
            if os.getenv('XTENSA_CORE')=='lx106':
                tool_nm = "xt-nm"
            proc = subprocess.Popen([tool_nm, self.name], stdout=subprocess.PIPE)
        except OSError:
            print "Error calling "+tool_nm+", do you have Xtensa toolchain in PATH?"
            sys.exit(1)
        for l in proc.stdout:
            fields = l.strip().split()
            self.symbols[fields[2]] = int(fields[0], 16)

    def get_symbol_addr(self, sym):
        self._fetch_symbols()
        return self.symbols[sym]

    def get_entry_point(self):
        tool_readelf = "xtensa-lx106-elf-readelf"
        if os.getenv('XTENSA_CORE')=='lx106':
            tool_objcopy = "xt-readelf"
        try:
            proc = subprocess.Popen([tool_readelf, "-h", self.name], stdout=subprocess.PIPE)
        except OSError:
            print "Error calling "+tool_nm+", do you have Xtensa toolchain in PATH?"
            sys.exit(1)
        for l in proc.stdout:
            fields = l.strip().split()
            if fields[0] == "Entry":
                return int(fields[3], 0);

    def load_section(self, section):
        tool_objcopy = "xtensa-lx106-elf-objcopy"
        if os.getenv('XTENSA_CORE')=='lx106':
            tool_objcopy = "xt-objcopy"
        subprocess.check_call([tool_objcopy, "--only-section", section, "-Obinary", self.name, ".tmp.section"])
        f = open(".tmp.section", "rb")
        data = f.read()
        f.close()
        os.remove(".tmp.section")
        return data


def arg_auto_int(x):
    return int(x, 0)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'ESP8266 ROM Bootloader Utility', prog = 'esptool')

    parser.add_argument(
            '--port', '-p',
            help = 'Serial port device',
            default = '/dev/ttyUSB0')

    parser.add_argument(
            '--baud', '-b',
            help = 'Serial port baud rate',
            type = arg_auto_int,
            default = ESPROM.ESP_ROM_BAUD)

    subparsers = parser.add_subparsers(
            dest = 'operation',
            help = 'Run esptool {command} -h for additional help')

    parser_load_ram = subparsers.add_parser(
            'load_ram',
            help = 'Download an image to RAM and execute')
    parser_load_ram.add_argument('filename', help = 'Firmware image')

    parser_dump_mem = subparsers.add_parser(
            'dump_mem',
            help = 'Dump arbitrary memory to disk')
    parser_dump_mem.add_argument('address', help = 'Base address', type = arg_auto_int)
    parser_dump_mem.add_argument('size', help = 'Size of region to dump', type = arg_auto_int)
    parser_dump_mem.add_argument('filename', help = 'Name of binary dump')

    parser_read_mem = subparsers.add_parser(
            'read_mem',
            help = 'Read arbitrary memory location')
    parser_read_mem.add_argument('address', help = 'Address to read', type = arg_auto_int)

    parser_write_mem = subparsers.add_parser(
            'write_mem',
            help = 'Read-modify-write to arbitrary memory location')
    parser_write_mem.add_argument('address', help = 'Address to write', type = arg_auto_int)
    parser_write_mem.add_argument('value', help = 'Value', type = arg_auto_int)
    parser_write_mem.add_argument('mask', help = 'Mask of bits to write', type = arg_auto_int)

    parser_write_flash = subparsers.add_parser(
            'write_flash',
            help = 'Write a binary blob to flash')
    parser_write_flash.add_argument('addr_filename', nargs = '+', help = 'Address and binary file to write there, separated by space')
    parser_write_flash.add_argument('--flash_freq', '-ff', help = 'SPI Flash frequency',
            choices = ['40m', '26m', '20m', '80m'], default = '40m')
    parser_write_flash.add_argument('--flash_mode', '-fm', help = 'SPI Flash mode',
            choices = ['qio', 'qout', 'dio', 'dout'], default = 'qio')
    parser_write_flash.add_argument('--flash_size', '-fs', help = 'SPI Flash size in Mbit',
            choices = ['4m', '2m', '8m', '16m', '32m'], default = '4m')

    parser_run = subparsers.add_parser(
            'run',
            help = 'Run application code in flash')

    parser_image_info = subparsers.add_parser(
            'image_info',
            help = 'Dump headers from an application image')
    parser_image_info.add_argument('filename', help = 'Image file to parse')

    parser_make_image = subparsers.add_parser(
            'make_image',
            help = 'Create an application image from binary files')
    parser_make_image.add_argument('output', help = 'Output image file')
    parser_make_image.add_argument('--segfile', '-f', action = 'append', help = 'Segment input file') 
    parser_make_image.add_argument('--segaddr', '-a', action = 'append', help = 'Segment base address', type = arg_auto_int) 
    parser_make_image.add_argument('--entrypoint', '-e', help = 'Address of entry point', type = arg_auto_int, default = 0)

    parser_elf2image = subparsers.add_parser(
            'elf2image',
            help = 'Create an application image from ELF file')
    parser_elf2image.add_argument('input', help = 'Input ELF file')
    parser_elf2image.add_argument('--output', '-o', help = 'Output filename prefix', type = str)
    parser_elf2image.add_argument('--flash_freq', '-ff', help = 'SPI Flash frequency',
            choices = ['40m', '26m', '20m', '80m'], default = '40m')
    parser_elf2image.add_argument('--flash_mode', '-fm', help = 'SPI Flash mode',
            choices = ['qio', 'qout', 'dio', 'dout'], default = 'qio')
    parser_elf2image.add_argument('--flash_size', '-fs', help = 'SPI Flash size in Mbit',
            choices = ['4m', '2m', '8m', '16m', '32m'], default = '4m')

    parser_read_mac = subparsers.add_parser(
            'read_mac',
            help = 'Read MAC address from OTP ROM')

    parser_flash_id = subparsers.add_parser(
            'flash_id',
            help = 'Read SPI flash manufacturer and device ID')

    parser_read_flash = subparsers.add_parser(
            'read_flash',
            help = 'Read SPI flash content')
    parser_read_flash.add_argument('address', help = 'Start address', type = arg_auto_int)
    parser_read_flash.add_argument('size', help = 'Size of region to dump', type = arg_auto_int)
    parser_read_flash.add_argument('filename', help = 'Name of binary dump')

    parser_erase_flash = subparsers.add_parser(
            'erase_flash',
            help = 'Perform Chip Erase on SPI flash')

    args = parser.parse_args()

    # Create the ESPROM connection object, if needed
    esp = None
    if args.operation not in ('image_info','make_image','elf2image'):
        esp = ESPROM(args.port, args.baud)
        esp.connect()

    # Do the actual work. Should probably be split into separate functions.
    if args.operation == 'load_ram':
        image = ESPFirmwareImage(args.filename)

        print 'RAM boot...'
        for (offset, size, data) in image.segments:
            print 'Downloading %d bytes at %08x...' % (size, offset),
            sys.stdout.flush()
            esp.mem_begin(size, math.ceil(size / float(esp.ESP_RAM_BLOCK)), esp.ESP_RAM_BLOCK, offset)

            seq = 0
            while len(data) > 0:
                esp.mem_block(data[0:esp.ESP_RAM_BLOCK], seq)
                data = data[esp.ESP_RAM_BLOCK:]
                seq += 1
            print 'done!'

        print 'All segments done, executing at %08x' % image.entrypoint
        esp.mem_finish(image.entrypoint)

    elif args.operation == 'read_mem':
        print '0x%08x = 0x%08x' % (args.address, esp.read_reg(args.address))

    elif args.operation == 'write_mem':
        esp.write_reg(args.address, args.value, args.mask, 0)
        print 'Wrote %08x, mask %08x to %08x' % (args.value, args.mask, args.address)

    elif args.operation == 'dump_mem':
        f = file(args.filename, 'wb')
        for i in xrange(args.size/4):
            d = esp.read_reg(args.address+(i*4))
            f.write(struct.pack('<I', d))
            if f.tell() % 1024 == 0:
                print '\r%d bytes read... (%d %%)' % (f.tell(), f.tell()*100/args.size),
                sys.stdout.flush()
        print 'Done!'

    elif args.operation == 'write_flash':
        assert len(args.addr_filename) % 2 == 0

        flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]
        flash_size_freq = {'4m':0x00, '2m':0x10, '8m':0x20, '16m':0x30, '32m':0x40}[args.flash_size]
        flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]
        flash_info = struct.pack('BB', flash_mode, flash_size_freq)

        while args.addr_filename:
            address = int(args.addr_filename[0], 0)
            filename = args.addr_filename[1]
            args.addr_filename = args.addr_filename[2:]
            image = file(filename, 'rb').read()
            print 'Erasing flash...'
            blocks = math.ceil(len(image)/float(esp.ESP_FLASH_BLOCK))
            esp.flash_begin(blocks*esp.ESP_FLASH_BLOCK, address)
            seq = 0
            while len(image) > 0:
                print '\rWriting at 0x%08x... (%d %%)' % (address + seq*esp.ESP_FLASH_BLOCK, 100*(seq+1)/blocks),
                sys.stdout.flush()
                block = image[0:esp.ESP_FLASH_BLOCK]
                # Fix sflash config data
                if address == 0 and seq == 0 and block[0] == '\xe9':
                    block = block[0:2] + flash_info + block[4:]
                # Pad the last block
                block = block + '\xff' * (esp.ESP_FLASH_BLOCK-len(block))
                esp.flash_block(block, seq)
                image = image[esp.ESP_FLASH_BLOCK:]
                seq += 1
            print
        print '\nLeaving...'
        if args.flash_mode == 'dio':
            esp.flash_unlock_dio()
        else:
            esp.flash_finish(False)

    elif args.operation == 'run':
        esp.run()

    elif args.operation == 'image_info':
        image = ESPFirmwareImage(args.filename)
        print ('Entry point: %08x' % image.entrypoint) if image.entrypoint != 0 else 'Entry point not set'
        print '%d segments' % len(image.segments)
        print
        checksum = ESPROM.ESP_CHECKSUM_MAGIC
        for (idx, (offset, size, data)) in enumerate(image.segments):
            print 'Segment %d: %5d bytes at %08x' % (idx+1, size, offset)
            checksum = ESPROM.checksum(data, checksum)
        print
        print 'Checksum: %02x (%s)' % (image.checksum, 'valid' if image.checksum == checksum else 'invalid!')

    elif args.operation == 'make_image':
        image = ESPFirmwareImage()
        if len(args.segfile) == 0:
            raise Exception('No segments specified')
        if len(args.segfile) != len(args.segaddr):
            raise Exception('Number of specified files does not match number of specified addresses')
        for (seg, addr) in zip(args.segfile, args.segaddr):
            data = file(seg, 'rb').read()
            image.add_segment(addr, data)
        image.entrypoint = args.entrypoint
        image.save(args.output)

    elif args.operation == 'elf2image':
        if args.output is None:
            args.output = args.input + '-'
        e = ELFFile(args.input)
        image = ESPFirmwareImage()
        image.entrypoint = e.get_entry_point()
        for section, start in ((".text", "_text_start"), (".data", "_data_start"), (".rodata", "_rodata_start")):
            data = e.load_section(section)
            image.add_segment(e.get_symbol_addr(start), data)

        image.flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]
        image.flash_size_freq = {'4m':0x00, '2m':0x10, '8m':0x20, '16m':0x30, '32m':0x40}[args.flash_size]
        image.flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]

        image.save(args.output + "0x00000.bin")
        data = e.load_section(".irom0.text")
        off = e.get_symbol_addr("_irom0_text_start") - 0x40200000
        assert off >= 0
        f = open(args.output + "0x%05x.bin" % off, "wb")
        f.write(data)
        f.close()

    elif args.operation == 'read_mac':
        mac = esp.read_mac()
        print 'MAC: %s' % ':'.join(map(lambda x: '%02x'%x, mac))

    elif args.operation == 'flash_id':
        flash_id = esp.flash_id()
        print 'Manufacturer: %02x' % (flash_id & 0xff)
        print 'Device: %02x%02x' % ((flash_id >> 8) & 0xff, (flash_id >> 16) & 0xff)

    elif args.operation == 'read_flash':
        print 'Please wait...'
        file(args.filename, 'wb').write(esp.flash_read(args.address, 1024, int(math.ceil(args.size / 1024.)))[:args.size])

    elif args.operation == 'erase_flash':
        esp.flash_erase()
