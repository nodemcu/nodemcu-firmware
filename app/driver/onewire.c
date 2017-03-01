/*
Adaptation of Paul Stoffregen's One wire library to the NodeMcu

The latest version of this library may be found at:
  http://www.pjrc.com/teensy/td_libs_OneWire.html

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Much of the code was inspired by Derek Yerger's code, though I don't
think much of that remains.  In any event that was..
    (copyleft) 2006 by Derek Yerger - Free to distribute freely.

The CRC code was excerpted and inspired by the Dallas Semiconductor
sample code bearing this copyright.
//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//--------------------------------------------------------------------------
*/

#include "driver/onewire.h"
#include "platform.h"
#include "osapi.h"

#define noInterrupts ets_intr_lock
#define interrupts ets_intr_unlock
#define delayMicroseconds os_delay_us

// 1 for keeping the parasitic power on H
#define owDefaultPower 1

#if ONEWIRE_SEARCH
// global search state
static unsigned char ROM_NO[NUM_OW][8];
static uint8_t LastDiscrepancy[NUM_OW];
static uint8_t LastFamilyDiscrepancy[NUM_OW];
static uint8_t LastDeviceFlag[NUM_OW];
#endif

void onewire_init(uint8_t pin)
{
	// pinMode(pin, INPUT);
  platform_gpio_mode(pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);
#if ONEWIRE_SEARCH
	onewire_reset_search(pin);
#endif
}


// Perform the onewire reset function.  We will wait up to 250uS for
// the bus to come high, if it doesn't then it is broken or shorted
// and we return a 0;
//
// Returns 1 if a device asserted a presence pulse, 0 otherwise.
//
uint8_t onewire_reset(uint8_t pin)
{
	uint8_t r;
	uint8_t retries = 125;

	noInterrupts();
	DIRECT_MODE_INPUT(pin);
	interrupts();
	// wait until the wire is high... just in case
	do {
		if (--retries == 0) return 0;
		delayMicroseconds(2);
	} while ( !DIRECT_READ(pin));

	noInterrupts();
	DIRECT_WRITE_LOW(pin);
	interrupts();
	delayMicroseconds(480);
	noInterrupts();
	DIRECT_MODE_INPUT(pin);	// allow it to float
	delayMicroseconds(70);
	r = !DIRECT_READ(pin);
	interrupts();
	delayMicroseconds(410);
	return r;
}

//
// Write a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
static void onewire_write_bit(uint8_t pin, uint8_t v, uint8_t power)
{
	if (v & 1) {
		noInterrupts();
		DIRECT_WRITE_LOW(pin);
		delayMicroseconds(5);
		if (power) {
			DIRECT_WRITE_HIGH(pin);
		} else {
			DIRECT_MODE_INPUT(pin);	// drive output high by the pull-up
		}
		delayMicroseconds(8);
		interrupts();
		delayMicroseconds(52);
	} else {
		noInterrupts();
		DIRECT_WRITE_LOW(pin);
		delayMicroseconds(65);
		if (power) {
			DIRECT_WRITE_HIGH(pin);
		} else {
			DIRECT_MODE_INPUT(pin);	// drive output high by the pull-up
		}
		interrupts();
		delayMicroseconds(5);
	}
}

//
// Read a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
static uint8_t onewire_read_bit(uint8_t pin)
{
	uint8_t r;

	noInterrupts();
	DIRECT_WRITE_LOW(pin);

	delayMicroseconds(5);
	DIRECT_MODE_INPUT(pin);	// let pin float, pull up will raise
	delayMicroseconds(8);
	r = DIRECT_READ(pin);
	interrupts();
	delayMicroseconds(52);
	return r;
}

//
// Write a byte. The writing code uses the external pull-up to raise the
// pin high, if you need power after the write (e.g. DS18S20 in
// parasite power mode) then set 'power' to 1 and the output driver will
// be activated at the end of the write. Otherwise the pin will
// go tri-state at the end of the write to avoid heating in a short or
// other mishap.
//
void onewire_write(uint8_t pin, uint8_t v, uint8_t power /* = 0 */) {
  uint8_t bitMask;

  for (bitMask = 0x01; bitMask; bitMask <<= 1) {
    // send last bit with requested power mode
    onewire_write_bit(pin, (bitMask & v)?1:0, bitMask & 0x80 ? power : 0);
  }
}

void onewire_write_bytes(uint8_t pin, const uint8_t *buf, uint16_t count, bool power /* = 0 */) {
  uint16_t i;
  for (i = 0 ; i < count ; i++)
    onewire_write(pin, buf[i], i < count-1 ? owDefaultPower : power);
}

//
// Read a byte
//
uint8_t onewire_read(uint8_t pin) {
  uint8_t bitMask;
  uint8_t r = 0;

  for (bitMask = 0x01; bitMask; bitMask <<= 1) {
  	if (onewire_read_bit(pin)) r |= bitMask;
  }
  return r;
}

void onewire_read_bytes(uint8_t pin, uint8_t *buf, uint16_t count) {
  uint16_t i;
  for (i = 0 ; i < count ; i++)
    buf[i] = onewire_read(pin);
}

//
// Do a ROM select
//
void onewire_select(uint8_t pin, const uint8_t rom[8])
{
    uint8_t i;

    onewire_write(pin, 0x55, owDefaultPower);           // Choose ROM

    for (i = 0; i < 8; i++) onewire_write(pin, rom[i], owDefaultPower);
}

//
// Do a ROM skip
//
void onewire_skip(uint8_t pin)
{
    onewire_write(pin, 0xCC, owDefaultPower);           // Skip ROM
}

void onewire_depower(uint8_t pin)
{
	noInterrupts();
	DIRECT_MODE_INPUT(pin);
	interrupts();
}

#if ONEWIRE_SEARCH

//
// You need to use this function to start a search again from the beginning.
// You do not need to do it for the first search, though you could.
//
void onewire_reset_search(uint8_t pin)
{
  // reset the search state
  LastDiscrepancy[pin] = 0;
  LastDeviceFlag[pin] = FALSE;
  LastFamilyDiscrepancy[pin] = 0;
  int i;
  for(i = 7; ; i--) {
    ROM_NO[pin][i] = 0;
    if ( i == 0) break;
  }
}

// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
//
void onewire_target_search(uint8_t pin, uint8_t family_code)
{
   // set the search state to find SearchFamily type devices
   ROM_NO[pin][0] = family_code;
   uint8_t i;
   for (i = 1; i < 8; i++)
      ROM_NO[pin][i] = 0;
   LastDiscrepancy[pin] = 64;
   LastFamilyDiscrepancy[pin] = 0;
   LastDeviceFlag[pin] = FALSE;
}

//
// Perform a search. If this function returns a '1' then it has
// enumerated the next device and you may retrieve the ROM from the
// OneWire::address variable. If there are no devices, no further
// devices, or something horrible happens in the middle of the
// enumeration then a 0 is returned.  If a new device is found then
// its address is copied to newAddr.  Use OneWire::reset_search() to
// start over.
//
// --- Replaced by the one from the Dallas Semiconductor web site ---
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
uint8_t onewire_search(uint8_t pin, uint8_t *newAddr)
{
   uint8_t id_bit_number;
   uint8_t last_zero, rom_byte_number, search_result;
   uint8_t id_bit, cmp_id_bit;

   unsigned char rom_byte_mask, search_direction;

   // initialize for search
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = 0;

   // if the last call was not the last one
   if (!LastDeviceFlag[pin])
   {
      // 1-Wire reset
      if (!onewire_reset(pin))
      {
         // reset the search
         LastDiscrepancy[pin] = 0;
         LastDeviceFlag[pin] = FALSE;
         LastFamilyDiscrepancy[pin] = 0;
         return FALSE;
      }

      // issue the search command
      onewire_write(pin, 0xF0, owDefaultPower);

      // loop to do the search
      do
      {
         // read a bit and its complement
         id_bit = onewire_read_bit(pin);
         cmp_id_bit = onewire_read_bit(pin);

         // check for no devices on 1-wire
         if ((id_bit == 1) && (cmp_id_bit == 1))
            break;
         else
         {
            // all devices coupled have 0 or 1
            if (id_bit != cmp_id_bit)
               search_direction = id_bit;  // bit write value for search
            else
            {
               // if this discrepancy if before the Last Discrepancy
               // on a previous next then pick the same as last time
               if (id_bit_number < LastDiscrepancy[pin])
                  search_direction = ((ROM_NO[pin][rom_byte_number] & rom_byte_mask) > 0);
               else
                  // if equal to last pick 1, if not then pick 0
                  search_direction = (id_bit_number == LastDiscrepancy[pin]);

               // if 0 was picked then record its position in LastZero
               if (search_direction == 0)
               {
                  last_zero = id_bit_number;

                  // check for Last discrepancy in family
                  if (last_zero < 9)
                     LastFamilyDiscrepancy[pin] = last_zero;
               }
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1)
              ROM_NO[pin][rom_byte_number] |= rom_byte_mask;
            else
              ROM_NO[pin][rom_byte_number] &= ~rom_byte_mask;

            // serial number search direction write bit
            onewire_write_bit(pin, search_direction, 0);

            // increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;

            // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
            if (rom_byte_mask == 0)
            {
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

      // if the search was successful then
      if (!(id_bit_number < 65))
      {
         // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
         LastDiscrepancy[pin] = last_zero;

         // check for last device
         if (LastDiscrepancy[pin] == 0)
            LastDeviceFlag[pin] = TRUE;

         search_result = TRUE;
      }
   }

   // if no device found then reset counters so next 'search' will be like a first
   if (!search_result || !ROM_NO[pin][0])
   {
      LastDiscrepancy[pin] = 0;
      LastDeviceFlag[pin] = FALSE;
      LastFamilyDiscrepancy[pin] = 0;
      search_result = FALSE;
   }
   else
   {
      for (rom_byte_number = 0; rom_byte_number < 8; rom_byte_number++)
      {
         newAddr[rom_byte_number] = ROM_NO[pin][rom_byte_number];
      }
   }
   return search_result;
}

#endif


#if ONEWIRE_CRC
// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
//

#if ONEWIRE_CRC8_TABLE
// This table comes from Dallas sample code where it is freely reusable,
// though Copyright (C) 2000 Dallas Semiconductor Corporation
static const uint8_t dscrc_table[] = {
      0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
    157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
     35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
    190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
     70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
    219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
    101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
    248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
    140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
     17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
    175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
     50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
    202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
     87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
    233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
    116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

//
// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (note: this might better be done without to
// table, it would probably be smaller and certainly fast enough
// compared to all those delayMicrosecond() calls.  But I got
// confused, so I use this table from the examples.)
//
uint8_t onewire_crc8(const uint8_t *addr, uint8_t len)
{
	uint8_t crc = 0;

	while (len--) {
		crc = pgm_read_byte(dscrc_table + (crc ^ *addr++));
	}
	return crc;
}
#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but much smaller, than the lookup table.
//
uint8_t onewire_crc8(const uint8_t *addr, uint8_t len)
{
	uint8_t crc = 0;

	while (len--) {
		uint8_t inbyte = *addr++;
    uint8_t i;
		for (i = 8; i; i--) {
			uint8_t mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}
#endif

#if ONEWIRE_CRC16
// Compute the 1-Wire CRC16 and compare it against the received CRC.
// Example usage (reading a DS2408):
    //    // Put everything in a buffer so we can compute the CRC easily.
//    uint8_t buf[13];
//    buf[0] = 0xF0;    // Read PIO Registers
//    buf[1] = 0x88;    // LSB address
//    buf[2] = 0x00;    // MSB address
//    WriteBytes(net, buf, 3);    // Write 3 cmd bytes
//    ReadBytes(net, buf+3, 10);  // Read 6 data bytes, 2 0xFF, 2 CRC16
//    if (!CheckCRC16(buf, 11, &buf[11])) {
//        // Handle error.
//    }
//
// @param input - Array of bytes to checksum.
// @param len - How many bytes to use.
// @param inverted_crc - The two CRC16 bytes in the received data.
//                       This should just point into the received data,
//                       *not* at a 16-bit integer.
// @param crc - The crc starting value (optional)
// @return True, iff the CRC matches.
bool onewire_check_crc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc)
{
    crc = ~onewire_crc16(input, len, crc);
    return (crc & 0xFF) == inverted_crc[0] && (crc >> 8) == inverted_crc[1];
}

// Compute a Dallas Semiconductor 16 bit CRC.  This is required to check
// the integrity of data received from many 1-Wire devices.  Note that the
// CRC computed here is *not* what you'll get from the 1-Wire network,
// for two reasons:
//   1) The CRC is transmitted bitwise inverted.
//   2) Depending on the endian-ness of your processor, the binary
//      representation of the two-byte return value may have a different
//      byte order than the two bytes you get from 1-Wire.
// @param input - Array of bytes to checksum.
// @param len - How many bytes to use.
// @param crc - The crc starting value (optional)
// @return The CRC16, as defined by Dallas Semiconductor.
uint16_t onewire_crc16(const uint8_t* input, uint16_t len, uint16_t crc)
{
    static const uint8_t oddparity[16] =
        { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

    uint16_t i;
    for (i = 0 ; i < len ; i++) {
      // Even though we're just copying a byte from the input,
      // we'll be doing 16-bit computation with it.
      uint16_t cdata = input[i];
      cdata = (cdata ^ crc) & 0xff;
      crc >>= 8;

      if (oddparity[cdata & 0x0F] ^ oddparity[cdata >> 4])
          crc ^= 0xC001;

      cdata <<= 6;
      crc ^= cdata;
      cdata <<= 1;
      crc ^= cdata;
    }
    return crc;
}
#endif

#endif
