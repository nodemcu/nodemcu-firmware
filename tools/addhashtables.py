#!/usr/bin/env python
#
# This tool adds the (constant) hash tables to the firmware image that enable
# fast lookups.
#
# This code uses some pieces of esptool.py which has the following license.
#
# Copyright (C) 2014-2016 Fredrik Ahlberg, Angus Gratton, other contributors as noted.
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

import argparse
import hashlib
import inspect
import json
import os
import struct
import subprocess
import sys
import tempfile
import time


__version__ = "0.1-dev"

def binutils_safe_path(p):
    """Returns a 'safe' version of path 'p' to pass to binutils

    Only does anything under Cygwin Python, where cygwin paths need to
    be translated to Windows paths if the binutils wasn't compiled
    using Cygwin (should also work with binutils compiled using
    Cygwin, see #73.)
    """
    if sys.platform == "cygwin":
        try:
            return subprocess.check_output(["cygpath", "-w", p]).rstrip('\n')
        except subprocess.CalledProcessError:
            print "WARNING: Failed to call cygpath to sanitise Cygwin path."
    return p



class ELFFile(object):
    def __init__(self, name):
        self.name = binutils_safe_path(name)
        self.symbols = None

    def _fetch_symbols(self):
        if self.symbols is not None:
            return
        self.symbols = {}
        try:
            tool_nm = "xtensa-lx106-elf-nm"
            if os.getenv('XTENSA_CORE') == 'lx106':
                tool_nm = "xt-nm"
            proc = subprocess.Popen([tool_nm, self.name], stdout=subprocess.PIPE)
        except OSError:
            print "Error calling %s, do you have Xtensa toolchain in PATH?" % tool_nm
            sys.exit(1)
        for l in proc.stdout:
            fields = l.strip().split()
            try:
                if fields[0] == "U":
                    print "Warning: ELF binary has undefined symbol %s" % fields[1]
                    continue
                if fields[0] == "w":
                    continue  # can skip weak symbols
                self.symbols[fields[2]] = int(fields[0], 16)
            except ValueError:
                raise FatalError("Failed to strip symbol output from nm: %s" % fields)

    def get_symbol_addr(self, sym):
        self._fetch_symbols()
        return self.symbols[sym]

    def foreach_symbol(self, fn):
        for symbol in self.symbols.iterkeys():
          fn(symbol)

    def get_entry_point(self):
        tool_readelf = "xtensa-lx106-elf-readelf"
        if os.getenv('XTENSA_CORE') == 'lx106':
            tool_readelf = "xt-readelf"
        try:
            proc = subprocess.Popen([tool_readelf, "-h", self.name], stdout=subprocess.PIPE)
        except OSError:
            print "Error calling %s, do you have Xtensa toolchain in PATH?" % tool_readelf
            sys.exit(1)
        for l in proc.stdout:
            fields = l.strip().split()
            if fields[0] == "Entry":
                return int(fields[3], 0)

    def load_section(self, section):
        tool_objcopy = "xtensa-lx106-elf-objcopy"
        if os.getenv('XTENSA_CORE') == 'lx106':
            tool_objcopy = "xt-objcopy"
        tmpsection = binutils_safe_path(tempfile.mktemp(suffix=".section"))
        try:
            subprocess.check_call([tool_objcopy, "--only-section", section, "-Obinary", self.name, tmpsection])
            with open(tmpsection, "rb") as f:
                data = f.read()
        except OSError, e:
            print "Error calling %s, do you have Xtensa toolchain in PATH?" % tool_nm
            sys.exit(1)
        finally:
            os.remove(tmpsection)
        return data

def luahash(str):
  # unsigned int h = cast(unsigned int, l);  /* seed */
  # size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  # size_t l1;
  # for (l1=l; l1>=step; l1-=step)  /* compute hash */
  #   h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1]));

  h = len(str)
  step = (len(str) >> 5) + 1
  for li in range(len(str) - 1, -1, -step):
    h = h ^ ((h << 5) + (h >> 2) + ord(str[li]))

  return h

class Image(object):
  imagedata = []

  def add_segment(self, addr, data):
    self.imagedata.append([addr, data])
    self.sortit()

  def sortit(self):
    self.imagedata = sorted(self.imagedata, key=lambda section: section[0])

  def find_section(self, addr):
    best = (None,None)
    for entry in self.imagedata:
      if addr >= entry[0]:
        best = entry
      else:
        return best

    return best
    
  def readstring(self, addr):
    base, data = self.find_section(addr)
    if base:
      data = data[addr - base : addr - base + 128]
      return data[:data.index("\0")]
    return None

  def read32(self, addr):
    base, data = self.find_section(addr)
    if not base:
      return None
    data = data[addr - base : addr - base + 4]
    if not data:
      return None
    return struct.unpack("i", data)[0]


def setoffset(hash, h, i):
    mask = len(hash) - 1

    while hash[h & mask]:
      hash[h & mask] = hash[h & mask] & 0x7f
      h = h + 1

    hash[h & mask] = 128 + i + 1

def dump_rotable(image, rotable, name, forcesize=None):
    # Count entries in tbl
    # name, value
    i = 0
    while image.read32(rotable + i * 16):
      i += 1

    print "// %s: %d entries" % (name, i)

    nents = 4
    while nents < i * 3:
      nents = nents << 1

    if nents > 256:
      nents = 256

    if forcesize > nents:
      nents = forcesize

    hash = [0] * nents
    i = 0
    while image.read32(rotable + i * 16):
      str = image.readstring(image.read32(rotable + i * 16))
      h = luahash(str) % nents
      setoffset(hash, h, i)

      i += 1

    dump_with_name(hash, name, nents)

dumped = {}

def dump_table(image, rotable=None, name=None, forcesize=None):
    if dumped.get(rotable):
      return
    dumped[rotable] = 1
    if not name:
        name = image.readstring(image.read32(rotable)).upper()
    tbl = image.read32(rotable + 4)

    # Count entries in tbl
    # name, value
    i = 0
    while image.read32(tbl + i * 16):
      i += 1

    print "// %s: %d entries" % (name, i)

    nents = 4
    while nents < i * 3:
      nents = nents << 1

    if nents > 256:
      nents = 256

    if forcesize > nents:
      nents = forcesize

    hash = [0] * nents
    i = 0
    while image.read32(tbl + i * 16):
      if (image.read32(tbl + i * 16) & 0xff) == 6:
        str = image.readstring(image.read32(tbl + i * 16 + 4))
        h = luahash(str) % nents
        setoffset(hash, h, i)

        if image.read32(tbl + i * 16 + 12) == 2:
          nexttbl = image.read32(tbl + i * 16 + 8)
          dump_table(image, rotable=nexttbl, name=image.readstring(image.read32(nexttbl)))

      i += 1

    dump_with_name(hash, name, nents)

def dump_with_name(hash, name, nents):
    print "const unsigned int %s_hashtable[] = { %d " % (name, nents - 1)
    print "// ", "".join(["," + hex(x) for x in hash])
    val = 0
    for i in range(0, nents):
      val = val + (hash[i] << (8 * (i & 3)))
      if (i & 3) == 3:
        print ",",hex(val),
        val = 0
    print "\n};"

def maybe_dump_table(image, symbol, addr):
    if symbol.endswith("_table"):
      name_addr = image.read32(addr)
      if name_addr:
        name = image.readstring(name_addr)
        if name and symbol.startswith(name):
          dump_table(image, addr, name)


def hashimage(args):
    e = ELFFile(args.input)

    # Start at "lua_rotable" and step through. 
    # Generate the C file with symbols MODULE_hashtable to replace the weak references

    image = Image()

    for section, start in ((".irom0.text", "_irom0_text_start"), (".text", "_text_start"), (".data", "_data_start"), (".rodata", "_rodata_start")):
        data = e.load_section(section)
        image.add_segment(e.get_symbol_addr(start), data)

    #luaR_table is name, table, stuff

    rotable = e.get_symbol_addr("lua_rotable")

    while image.read32(rotable):
      dump_table(image, rotable)
      rotable += 16

    dump_table(image, e.get_symbol_addr("lua_base_funcs_table"), "lua_base_funcs_table", forcesize=256)
    dump_table(image, e.get_symbol_addr("strlib_table"), "strlib_table")
    dump_rotable(image, e.get_symbol_addr("lua_rotable"), "lua_rotable_table", forcesize=256)

    e.foreach_symbol(lambda symbol: maybe_dump_table(image, symbol, e.get_symbol_addr(symbol)))


class FatalError(RuntimeError):
    """
    Wrapper class for runtime errors that aren't caused by internal bugs, but by
    ESP8266 responses or input content.
    """
    def __init__(self, message):
        RuntimeError.__init__(self, message)

    @staticmethod
    def WithResult(message, result):
        """
        Return a fatal error object that includes the hex values of
        'result' as a string formatted argument.
        """
        return FatalError(message % ", ".join(hex(ord(x)) for x in result))


def version(args):
    print __version__

#
# End of operations functions
#


def main():
    parser = argparse.ArgumentParser(description='addhashtables.py v%s - NodeMCU firmware patches' % __version__, prog='addhashtables')
    parser.add_argument('input', help='Input ELF file')

    args = parser.parse_args()

    # print 'esptool.py v%s' % __version__

    # operation function can take 1 arg (args), 2 args (esp, arg)
    # or be a member function of the ESPROM class.

    hashimage(args)


if __name__ == '__main__':
    try:
        main()
    except FatalError as e:
        print '\nA fatal error occurred: %s' % e
        sys.exit(2)
