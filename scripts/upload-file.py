#!/usr/bin/env python3

# A helper utility to allow uploading of files to NodeMCU versions which use
# the 'console' module, rather than having the console multiplexed via the
# 'uart' module.

import argparse
import serial
import sys
import atexit

STX = 0x02
ETX = 0x03
DLE = 0x10

# The loader we send to NodeMCU so that we may upload a (binary) file safely.
# Uses STX/ETX/DLE framing and escaping.
loader = b'''
(function()
  local function transmission_receiver(chunk_cb)
    local inframe = false
    local escaped = false
    local done = false
    local STX = 2
    local ETX = 3
    local DLE = 16
    local function dispatch(data, i, j)
      if (j - i) < 0 then return end
      chunk_cb(data:sub(i, j))
    end
    return function(data)
      if done then return end
      local from
      local to
      for i = 1, #data
      do
        local b = data:byte(i)
        if inframe
        then
          if not from then from = i end -- first valid byte
          if escaped
          then
            escaped = false
          else
            if b == DLE
            then
              escaped = true
              dispatch(data, from, i-1)
              from = nil
            elseif b == ETX
            then
              done = true
              to = i-1
              break
            end
          end
        else -- look for an (unescaped) STX to sync frame start
          if b == DLE then escaped = true
          elseif b == STX and not escaped then inframe = true
          else escaped = false
          end
        end
        -- else ignore byte outside of framing
      end
      if from then dispatch(data, from, to or #data) end
      if done then chunk_cb(nil) end
    end
  end

  local function file_saver(name)
    local f = io.open(name, "w")
    return function(chunk)
      if chunk then f:write(chunk)
      else
        f:close()
        console.on("data", 0, nil)
        console.mode(console.INTERACTIVE)
      end
    end
  end

  console.on("data", 0, transmission_receiver(file_saver("@FILENAME@")))
  console.mode(console.NONINTERACTIVE)
end)()
'''

def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="NodeMCU file uploader.")
    parser.add_argument("file", help="File to read data from.")
    parser.add_argument("name", nargs="?", help="Name to upload file as.")
    parser.add_argument("-p", "--port", default="/dev/ttyUSB0", help="Serial port (default: /dev/ttyUSB0).")
    parser.add_argument("-b", "--bitrate", type=int, default=115200, help="Bitrate (default: 115200).")
    return parser.parse_args()

def load_file(filename):
    """Open a file and read its contents into memory."""
    try:
        with open(filename, "r") as f:
            data = f.read()
        return data
    except IOError as e:
        print(f"Error reading file {filename}: {e}")
        sys.exit(1)

def wait_prompt(ser):
    """Wait until we see the '> ' prompt, or the serial times out"""
    buf = bytearray()
    b = ser.read()
    while b != b'':
        buf.extend(b)
        if buf.find(b'> ') != -1:
            return True
        b = ser.read()
    return False

def sync(ser):
    """Get ourselves to a clean prompt so we can understand the output"""
    ser.write(b'\x03\x03\n')
    wait_prompt(ser)
    ser.write(b"print('sync')\n")
    line = ser.readline()
    while line != b"sync\n":
       line = ser.readline()
    return wait_prompt(ser)

def cleanup():
    """Cleanup function to send final data and close the serial port."""
    if ser:
        # Ensure we don't leave the console in a weird state if we get
        # interrupted.
        ser.write(ETX)
        ser.write(ETX)
        ser.write(b"\n")
        ser.readline()
        ser.close()

def line_interactive_send(ser, data):
    """Send one line at a time, waiting for the prompt before sending next"""
    for line in data.split(b'\n'):
        ser.write(line)
        ser.write(b'\n')
        wait_prompt(ser)

def transmission(data):
    """Perform STX/ETX/DLE framing and escaping of the data"""
    out = bytearray()
    out.append(STX)
    for b in data:
        if b == STX or b == ETX or b == DLE:
            out.append(DLE)
        out.append(ord(b))
    out.append(ETX)
    return bytes(out)

if __name__ == "__main__":
    args = parse_args()

    upload_name = args.name if args.name else args.file

    file_data = load_file(args.file)

    try:
        ser = serial.Serial(args.port, args.bitrate, timeout=1)
    except serial.SerialException as e:
        print(f"Error opening serial port {args.port}: {e}")
        sys.exit(1)

    print("Synchronising serial...")
    if not sync(ser):
        print("NodeMCU not responding\n")
        sys.exit(1)

    print(f'Uploading "{args.file}" as "{upload_name}"')

    atexit.register(cleanup)

    print("Sending loader...")
    line_interactive_send(
        ser, loader.replace(b"@FILENAME@", upload_name.encode()))

    print("Sending file contents...")
    ser.write(transmission(file_data))
    wait_prompt(ser)

    ser.close()
    ser = None
    print("Done.")
