#!/usr/bin/env python
#
# ESP8266 Firmware Utility
#
# Derived from 'esptool' (https://github.com/themadinventor/esptool)
#
# Copyright (C) 2014 Fredrik Ahlberg (original esptool)
# Copyright (C) 2015 DiUS Computing Pty Ltd
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

class ESPFirmwareImage:

    # First byte of the application image
    ESP_IMAGE_MAGIC = 0xe9

    # Initial state for the checksum routine
    ESP_CHECKSUM_MAGIC = 0xef

    """ Calculate checksum of a blob, as it is defined by the ROM """
    @staticmethod
    def checksum(data, state = ESP_CHECKSUM_MAGIC):
        for b in data:
            state ^= ord(b)
        return state

    def __init__(self, filename = None):
        self.segments = []
        self.entrypoint = 0
        self.flash_mode = 0
        self.flash_size_freq = 0

        if filename is not None:
            f = file(filename, 'rb')
            (magic, segments, self.flash_mode, self.flash_size_freq, self.entrypoint) = struct.unpack('<BBBBI', f.read(8))
            
            # some sanity check
            if magic != ESPFirmwareImage.ESP_IMAGE_MAGIC or segments > 16:
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
        if l > 0:
            if l % 4:
                data += b"\x00" * (4 - l % 4)
            self.segments.append((addr, len(data), data))

    def save(self, filename):
        f = file(filename, 'wb')
        f.write(struct.pack('<BBBBI', ESPFirmwareImage.ESP_IMAGE_MAGIC, len(self.segments),
            self.flash_mode, self.flash_size_freq, self.entrypoint))

        checksum = ESPFirmwareImage.ESP_CHECKSUM_MAGIC
        for (offset, size, data) in self.segments:
            f.write(struct.pack('<II', offset, size))
            f.write(data)
            checksum = ESPFirmwareImage.checksum(data, checksum)

        align = 15-(f.tell() % 16)
        f.seek(align, 1)
        f.write(struct.pack('B', checksum))
        return f


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
            print "Error calling "+tool_readelf+", do you have Xtensa toolchain in PATH?"
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


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'ESP8266 Firmware Utility', prog = 'mkespimage')

    subparsers = parser.add_subparsers(
        dest = 'operation',
        help = 'Run mkespimage {command} -h for additional help')

    parser_image_info = subparsers.add_parser(
        'image_info',
        help = 'Dump headers from a firmware image')
    parser_image_info.add_argument('filename', help = 'Image file to parse')

    parser_elf2image = subparsers.add_parser(
        'elf2image',
        help = 'Create a firmware image from an ELF file')
    parser_elf2image.add_argument('input', help = 'Input ELF file')
    parser_elf2image.add_argument('--output', '-o', help = 'Output filename prefix', type = str)
    parser_elf2image.add_argument('--format', '-F', help = 'Output format',
        choices = ['combined', 'c', 'split', 's' ], default = 'combined')
    parser_elf2image.add_argument('--flash_freq', '-ff', help = 'SPI Flash frequency',
        choices = ['40m', '26m', '20m', '80m'], default = '40m')
    parser_elf2image.add_argument('--flash_mode', '-fm', help = 'SPI Flash mode',
        choices = ['qio', 'qout', 'dio', 'dout'], default = 'qio')
    parser_elf2image.add_argument('--flash_size', '-fs', help = 'SPI Flash size in Mbit',
        choices = ['4m', '2m', '8m', '16m', '32m'], default = '4m')

    args = parser.parse_args()

    if args.operation == 'image_info':
        image = ESPFirmwareImage(args.filename)
        print ('Entry point: %08x' % image.entrypoint) if image.entrypoint != 0 else 'Entry point not set'
        print '%d segments' % len(image.segments)
        print
        checksum = ESPFirmwareImage.ESP_CHECKSUM_MAGIC
        for (idx, (offset, size, data)) in enumerate(image.segments):
            print 'Segment %d: %5d bytes at %08x' % (idx+1, size, offset)
            checksum = ESPFirmwareImage.checksum(data, checksum)
        print
        print 'Checksum: %02x (%s)' % (image.checksum, 'valid' if image.checksum == checksum else 'invalid!')

    elif args.operation == 'elf2image':
        if args.output is None:
            args.output = args.input + '-'
        e = ELFFile(args.input)
        image = ESPFirmwareImage()
        image.entrypoint = e.get_entry_point()
        for section, start in ( (".data", "_data_start"), (".text", "_text_start"), (".rodata", "_rodata_start")):
            data = e.load_section(section)
            image.add_segment(e.get_symbol_addr(start), data)

        image.flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]
        image.flash_size_freq = {'4m':0x00, '2m':0x10, '8m':0x20, '16m':0x30, '32m':0x40}[args.flash_size]
        image.flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]

        combined = { 'split':0, 's': 0, 'combined': 1, 'c':1 }[args.format]

        f = image.save(args.output + "0x00000.bin")
        data = e.load_section(".irom0.text")
        off = e.get_symbol_addr("_irom0_text_start") - 0x40200000
        assert off >= 0
        if combined:
            f.seek(off)
        else:
            f.close()
            f = open(args.output + "0x%05x.bin" % off, "wb")
        f.write(data)
        f.close()
