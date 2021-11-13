#!/usr/bin/env python3

import sys
import os
import struct

# [volume records]
# reclen, index_offs, name
# reclen, index_offs2, name2
# [file index]
# reclen, offs, file_len, name   # offs base index_offs
# reclen, offs, file_len, name2  # offs base index_offs2 
# [file contents]
# rawdata
# rawdata

vol_names = [] # one entry per volume
volume_indexes = [] # one entry per volume
volume_file_contents = [] # one entry per volume

for voldef in sys.argv[1:]:
    [ name, basedir ] = voldef.split('=')
    print(f'==> Packing volume "{name}" from {basedir}')
    vol_names.append(name)
    # Make relative paths relative to the top nodemcu-firmware dir; this
    # script gets executed with build/esp-idf/modules as the current dir
    if not os.path.isabs(basedir):
        basedir = os.path.join(*['..', '..', '..', basedir])
    if not os.path.isdir(basedir):
        raise FileNotFoundError(f'source directory {basedir} not found')
    basedir_len = len(basedir) +1
    file_index = b''
    file_data = b''
    offs = 0
    entries = []
    index_size = 0
    for root, subdirs, files in os.walk(basedir):
        prefix = ('' if root == basedir else root[basedir_len:] + '/')
        for filename in files:
            hostrelpath = os.path.join(root, filename)
            relpath = prefix + filename
            size = os.path.getsize(hostrelpath)
            rec_len = 1 + 4 + 4 + len(relpath) # reclen + offs + filelen + name
            if rec_len > 255:
                raise ValueError(f'excessive path length for {relpath}')
            entries.append([ rec_len, offs, size, relpath ])
            offs += size
            index_size += rec_len
            with open(hostrelpath, mode='rb') as f:
                file_data += f.read()
    for entry in entries:
        [ rec_len, offs, size, relpath ] = entry
        print('[', rec_len, index_size + offs, size, relpath, ']')
        file_index += \
            struct.pack('<BII', rec_len, index_size + offs, size) + \
            relpath.encode('utf-8')

    volume_indexes.append(file_index)
    volume_file_contents.append(file_data)

volume_records_len = len(vol_names) * (1 + 2) + len(''.join(vol_names))
print(f'==> Generating volumes index ({volume_records_len} bytes)')
with open('eromfs.bin', 'wb') as f:
    index_offs = volume_records_len
    for idx, name in enumerate(vol_names):
        rec_len = 1 + 2 + len(name)
        index_len = len(volume_indexes[idx])
        data_len = len(volume_file_contents[idx])
        if rec_len > 255:
            raise ValueError(f'volume name too long for {name}')
        if index_offs > 65535:
            raise ValueError('volumes index overflowed; too many volumes')
        f.write(
            struct.pack('<BH', rec_len, index_offs) + name.encode('utf-8') \
        )
        print(f'- {name} (index {index_len} bytes; content {data_len} bytes)')
        index_offs += index_len + data_len
    for idx, index in enumerate(volume_indexes):
        f.write(index)
        f.write(volume_file_contents[idx])
