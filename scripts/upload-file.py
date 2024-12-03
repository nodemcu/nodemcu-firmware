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
# The CDC-ACM console gets overwhelmed unless we throttle the send by using
# an ack scheme. We use a fake prompt for simplicity's sake for that.
loader = b'''
(function()
  local function transmission_receiver(chunk_cb)
    local inframe = false
    local escaped = false
    local done = false
    local len = 0
    local STX = 2
    local ETX = 3
    local DLE = 16
    local function dispatch(data, i, j)
      if (j - i) < 0 then return end
      chunk_cb(data:sub(i, j))
    end
    return function(data)
      if done then return end
      len = len + #data
      while len >= @BLOCKSIZE@ do
        len = len - @BLOCKSIZE@
        console.write("> ")
      end
      local from
      local to
      for i = 1, #data
      do
        local b = data:byte(i)
        if inframe
        then
          if not from then from = i end -- first valid byte
          if escaped then escaped = false else
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
          else escaped = false end
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
      if chunk then f:write(chunk) else
        f:close()
        console.on("data", 0, nil)
        console.mode(console.INTERACTIVE)
        console.write("done")
      end
    end
  end

  console.on("data", 0, transmission_receiver(file_saver(
    "@FILENAME@")))
  console.mode(console.NONINTERACTIVE)
  console.write("ready")
end)()
'''

def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="NodeMCU file uploader.")
    parser.add_argument("file", help="File to read data from.")
    parser.add_argument("name", nargs="?", help="Name to upload file as.")
    parser.add_argument("-p", "--port", default="/dev/ttyUSB0", help="Serial port (default: /dev/ttyUSB0).")
    parser.add_argument("-b", "--bitrate", type=int, default=115200, help="Bitrate (default: 115200).")
    parser.add_argument("-s", "--blocksize", type=int, default=80, help="Block size of file data, tweak for speed/reliability of upload (default: 80)")
    return parser.parse_args()

def load_file(filename):
    """Open a file and read its contents into memory."""
    try:
        with open(filename, "rb") as f:
            data = f.read()
        return data
    except IOError as e:
        print(f"Error reading file {filename}: {e}")
        sys.exit(1)

def xprint(msg):
    print(msg, end='', flush=True)

def wait_prompt(ser, ignore):
    """Wait until we see the '> ' prompt, or the serial times out"""
    buf = bytearray()
    b = ser.read()
    timeout = 5
    while timeout > 0:
        if b == b'':
            timeout -= 1
            xprint('!')
        else:
            buf.extend(b)
        if not ignore and buf.find(b'Lua error:') != -1:
            xprint(buf.decode())
            line = ser.readline()
            while line != b'':
                xprint(line.decode())
                line = ser.readline()
            sys.exit(1)
        if buf.find(b'> ') != -1:
            return True
        b = ser.read()
    xprint(buf.decode())
    return False

def wait_line_match(ser, match, timeout):
    """Wait until the 'match' string is found within a line, or times out"""
    line = ser.readline()
    while timeout > 0:
        if line.find(match) != -1:
            return True
        elif line == b'':
            timeout -= 1
            xprint('!')
    return False

def sync(ser):
    """Get ourselves to a clean prompt so we can understand the output"""
    ser.write(b'\x03\x03\n')
    if not wait_prompt(ser, True):
        return False
    ser.write(b"print('sync')\n")
    return wait_line_match(ser, b'sync', 5) and wait_prompt(ser, True)

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
        if not wait_prompt(ser, False):
            return False
        xprint('.')
    return True

def chunk_data(data, size):
    """Split a data block into chunks"""
    return (data[0+i:size+i] for i in range(0, len(data), size))

def chunk_interactive_send(ser, data, size):
    """Send the data chunked into blocks, waiting for an ack in between"""
    n=0
    for chunk in chunk_data(data, size):
        ser.write(chunk)
        if len(chunk) == size and not wait_prompt(ser, False):
            print(f"failed after sending {n} blocks")
            return False
        xprint('.')
        n += 1
    print(f" ok, sent {n} blocks")
    return True

def transmission(data):
    """Perform STX/ETX/DLE framing and escaping of the data"""
    out = bytearray()
    out.append(STX)
    for b in data:
        if b == STX or b == ETX or b == DLE:
            out.append(DLE)
        out.append(b)
    out.append(ETX)
    return bytes(out)

if __name__ == "__main__":
    args = parse_args()

    upload_name = args.name if args.name else args.file

    file_data = load_file(args.file)
    print(f"Loaded {len(file_data)} bytes of file contents")

    blocksize = bytes(str(args.blocksize).encode())

    try:
        ser = serial.Serial(port=args.port, baudrate=args.bitrate, timeout=1)
    except serial.SerialException as e:
        print(f"Error opening serial port {args.port}: {e}")
        sys.exit(1)

    print("Synchronising serial...", end='')
    if not sync(ser):
        print("\nNodeMCU not responding\n")
        sys.exit(1)

    print(f' ok\nUploading "{args.file}" as "{upload_name}"')

    atexit.register(cleanup)

    xprint("Sending loader")
    ok = line_interactive_send(
        ser, loader.replace(
            b"@FILENAME@", upload_name.encode()).replace(
            b"@BLOCKSIZE@", blocksize))

    if ok:
        xprint(" ok\nWaiting for go-ahead...")
        ok = wait_line_match(ser, b"ready", 5)

    if ok:
        xprint(f" ok\nSending file contents (using blocksize {args.blocksize})")
        ok = chunk_interactive_send(
            ser, transmission(file_data), int(blocksize))
    if ok:
        xprint("Waiting for final ack...")
        ok = wait_line_match(ser, b"done", 5)
        ser.write(b"\n")

    if not ok or not wait_prompt(ser, False):
        print("transmission timed out")
        sys.exit(1)

    ser.close()
    ser = None
    print(" ok\nUpload complete.")
