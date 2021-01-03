uzlib - Deflate/Zlib-compatible LZ77 compression library
======================================================

This is a heavily modified and cut down version of Paul Sokolovsky's
uzlib library. This library has exported routines which

-  Can compress data to a Deflate-compatible bitstream, albeit with lower
compression ratio than the Zlib Deflate algorithm as a static Deflate Huffman
tree encoding is used for bitstream). Note that since this compression is
in RAM and requires ~4 bytes per byte of the input record, should only be
called for compressing small records on the ESP8266.

-  Can decompress any valid Deflate, Zlib, and Gzip (further called just
"Deflate") bitstream less than 16Kb, and any arbitrary length stream
compressed by the uzlib compressor.

uzlib aims for minimal code size and runtime memory requirements, and thus
is suitable for embedded systems and IoT devices such as the ESP8266.

uzlib is based on:

-  tinf library by Joergen Ibsen (Deflate decompression)
-  Deflate Static Huffman tree routines by Simon Tatham
-  LZ77 compressor by Paul Sokolovsky provided my initial inspiration, but
I ended up rewriting this following RFC 1951 to get improved compression
performance.

The above 16Kb limitation arises from the RFC 1951 use of a 32Kb dictionary,
which is impractical on a chipset with only ~40 Kb RAM avialable to
applications.

The relevant copyright statements are provided in the source files which
use this code.

uzlib library is licensed under Zlib license.
