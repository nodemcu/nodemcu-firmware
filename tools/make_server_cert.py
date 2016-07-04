import os
import argparse
import base64
import re
import sys


class Cert(object):
    def __init__(self, name, buff):
        self.name = name
        self.len = len(buff)
        self.buff = buff
        pass
    
    def __str__(self):
        out_str = ['\0']*32
        for i in range(len(self.name)):
            out_str[i] = self.name[i]
        out_str = "".join(out_str)
        out_str += str(chr(self.len & 0xFF))
        out_str += str(chr((self.len & 0xFF00) >> 8))
        out_str += self.buff
        return out_str


def main():
    parser = argparse.ArgumentParser(description='Convert PEM file(s) into C source file.')

    parser.add_argument('--section', 
                   default='.servercert.flash',
		    help='specify the section for the data (default is .servercert.flash)')

    parser.add_argument('--name', 
                   default='tls_server_cert_area',
		    help='specify the variable name for the data (default is tls_server_cert_area)')

    parser.add_argument('file', nargs='+',
		    help='One or more PEM files')

    args = parser.parse_args()

    cert_list = []
    cert_file_list = []

    for cert_file in args.file:
        with open(cert_file, 'r') as f:
            buff = f.read()
	m = re.search(r"-----BEGIN ([A-Z ]+)-----([^-]+?)-----END \1-----", buff, flags=re.DOTALL)
	if not m:
	    sys.exit("Input file was not in PEM format")

	if "----BEGIN" in buff[m.end(0):]:
	    sys.exit("Input file contains more than one PEM object")

        cert_list.append(Cert(m.group(1), base64.b64decode(''.join(m.group(2).split()))))

    print '__attribute__((section("%s"))) unsigned char %s[INTERNAL_FLASH_SECTOR_SIZE] = {' % (args.section, args.name)
    for _cert in cert_list:
	col = 0
	for ch in str(_cert):
	    print ("0x%02x," % ord(ch)),
	    if col & 15 == 15:
	      print
	    col = col + 1
    print '\n0xff};\n'

if __name__ == '__main__':
    main()

