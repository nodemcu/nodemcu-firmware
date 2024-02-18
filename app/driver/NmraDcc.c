//------------------------------------------------------------------------
//
// Model Railroading with Arduino - NmraDcc.cpp 
//
// Copyright (c) 2008 - 2020 Alex Shepherd
//
// 	This library is free software; you can redistribute it and/or
// 	modify it under the terms of the GNU Lesser General Public
// 	License as published by the Free Software Foundation; either
// 	version 2.1 of the License, or (at your option) any later version.
// 
// 	This library is distributed in the hope that it will be useful,
// 	but WITHOUT ANY WARRANTY; without even the implied warranty of
// 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// 	Lesser General Public License for more details.
// 
// 	You should have received a copy of the GNU Lesser General Public
// 	License along with this library; if not, write to the Free Software
// 	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// 
//------------------------------------------------------------------------
//
// file:      NmraDcc.cpp
// author:    Alex Shepherd
// webpage:   http://mrrwa.org/
// history:   2008-03-20 Initial Version
//            2011-06-26 Migrated into Arduino library from OpenDCC codebase
//            2014 Added getAddr to NmraDcc  Geoff Bunza
//            2015-11-06 Martin Pischky (martin@pischky.de):
//                       Experimental Version to support 14 speed steps
//                       and new signature of notifyDccSpeed and notifyDccFunc
//            2015-12-16 Version without use of Timer0 by Franz-Peter Müller
//            2016-07-16 handle glitches on DCC line
//			  2016-08-20 added ESP8266 support by Sven (littleyoda) 
//			  2017-01-19 added STM32F1 support by Franz-Peter
//            2017-11-29 Ken West (kgw4449@gmail.com):
//                       Minor fixes to pass NMRA Baseline Conformance Tests.
//            2018-12-17 added ESP32 support by Trusty (thierry@lapajaparis.net)
//            2019-02-17 added ESP32 specific changes by Hans Tanner
//            2020-05-15 changes to pass NMRA Tests ( always search for preamble )
//------------------------------------------------------------------------
//
// purpose:   Provide a simplified interface to decode NMRA DCC packets
//			  and build DCC Mobile and Stationary Decoders
//
//------------------------------------------------------------------------

// NodeMCU Lua port by @voborsky

// #define NODE_DEBUG
#define NODEMCUDCC

#ifdef NODEMCUDCC
  #include <stdint.h>
  #include <stdlib.h>
  #include <stdio.h>
  #include "platform.h"
  #include "user_interface.h"
  #include "task/task.h"
  #include "driver/NmraDcc.h"

  #define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
  #define BYTE_TO_BINARY(byte)  \
    (byte & 0x80 ? '1' : '0'), \
    (byte & 0x40 ? '1' : '0'), \
    (byte & 0x20 ? '1' : '0'), \
    (byte & 0x10 ? '1' : '0'), \
    (byte & 0x08 ? '1' : '0'), \
    (byte & 0x04 ? '1' : '0'), \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0')
   
   #define byte uint8_t
   #define word int16_t

   #define abs(a) ((a) > 0 ? (a) : (0-a))
   
   #define RISING GPIO_PIN_INTR_POSEDGE
   #define FALLING GPIO_PIN_INTR_NEGEDGE
   #define CHANGE GPIO_PIN_INTR_ANYEDGE
   
    static uint32_t last_time_overflow_millis;
    static uint32_t last_system_time;

    uint32_t millis() {
      uint32_t now = system_get_time();

      if (now < last_system_time) {
        // we have an overflow situation 
        // assume only one overflow
        last_time_overflow_millis += (1 << 29) / 125;   // (1 << 32) / 1000
      }

      last_system_time = now;
      return last_time_overflow_millis + now / 1000;
    }
#else
#include "NmraDcc.h"
#include "EEPROM.h"
#endif

// Uncomment to print DEBUG messages
// #define DEBUG_PRINT		

//------------------------------------------------------------------------
// DCC Receive Routine
//
// Howto:    uses two interrupts: a rising edge in DCC polarity triggers INTx
//           in INTx handler, Timer0 CompareB with a delay of 80us is started.
//           On Timer0 CompareB Match the level of DCC is evaluated and
//           parsed.
//
//                           |<-----116us----->|
//
//           DCC 1: _________XXXXXXXXX_________XXXXXXXXX_________
//                           ^-INTx
//                           |----87us--->|
//                                        ^Timer-INT: reads zero
//
//           DCC 0: _________XXXXXXXXXXXXXXXXXX__________________
//                           ^-INTx
//                           |----------->|
//                                        ^Timer-INT: reads one
//           
// new DCC Receive Routine without Timer0 ........................................................
//
// Howto:    uses only one interrupt at the rising or falling edge of the DCC signal
//           The time between two edges is measured to determine the bit value
//           Synchronising to the edge of the first part of a bit is done after recognizing the start bit
//           During synchronizing each part of a bit is detected ( Interruptmode 'change' )
//
//                           |<-----116us----->|
//           DCC 1: _________XXXXXXXXX_________XXXXXXXXX_________
//                           |<--------146us------>|
//                           ^-INTx            ^-INTx
//                           less than 146us: its a one-Bit
//                                        
//
//                           |<-----------------232us----------->|
//           DCC 0: _________XXXXXXXXXXXXXXXXXX__________________XXXXXXXX__________
//                           |<--------146us------->|
//                           ^-INTx                              ^-INTx
//                           greater than 146us: its a zero bit
//                                        
//                                        
//                                           
//           

//------------------------------------------------------------------------
// if this is commented out, bit synchronisation is only done after a wrong checksum
#define SYNC_ALWAYS

// if this is commented out, Zero-Bit_Stretching is not supported
// ( Bits longer than 2* MAX ONEBIT are treated as error )
#define SUPPORT_ZERO_BIT_STRETCHING

#define MAX_ONEBITFULL  146
#define MAX_PRAEAMBEL   146 
#define MAX_ONEBITHALF  82
#define MIN_ONEBITFULL  82
#define MIN_ONEBITHALF  35
#define MAX_BITDIFF     24


// Debug-Ports
//#define debug     // Testpulse for logic analyser
#ifdef NODE_DEBUG
    #define debug
#endif
#ifdef debug 
    #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
        #define MODE_TP1 DDRF |= (1<<2) //pinA2
        #define SET_TP1 PORTF |= (1<<2)
        #define CLR_TP1 PORTF &= ~(1<<2)
        #define MODE_TP2 DDRF |= (1<<3) //pinA3
        #define SET_TP2 PORTF |= (1<<3)
        #define CLR_TP2 PORTF &= ~(1<<3)
        #define MODE_TP3 DDRF |= (1<<4) //pinA4 
        #define SET_TP3 PORTF |= (1<<4) 
        #define CLR_TP3 PORTF &= ~(1<<4) 
        #define MODE_TP4 DDRF |= (1<<5) //pinA5 
        #define SET_TP4 PORTF |= (1<<5) 
        #define CLR_TP4 PORTF &= ~(1<<5) 
    #elif defined(__AVR_ATmega32U4__)
        #define MODE_TP1 DDRF |= (1<<4) //A3
        #define SET_TP1 PORTF |= (1<<4)
        #define CLR_TP1 PORTF &= ~(1<<4)
        #define MODE_TP2 DDRF |= (1<<5) //A2
        #define SET_TP2 PORTF |= (1<<5)
        #define CLR_TP2 PORTF &= ~(1<<5)
        #define MODE_TP3 
        #define SET_TP3 
        #define CLR_TP3 
        #define MODE_TP4 
        #define SET_TP4 
        #define CLR_TP4 
    #elif defined(__AVR_ATmega328P__) 
        #define MODE_TP1 DDRC |= (1<<1) //A1
        #define SET_TP1 PORTC |= (1<<1)
        #define CLR_TP1 PORTC &= ~(1<<1)
        #define MODE_TP2 DDRC |= (1<<2) // A2
        #define SET_TP2 PORTC |= (1<<2)
        #define CLR_TP2 PORTC &= ~(1<<2)
        #define MODE_TP3 DDRC |= (1<<3) //A3
        #define SET_TP3 PORTC |= (1<<3) 
        #define CLR_TP3 PORTC &= ~(1<<3) 
        #define MODE_TP4 DDRC |= (1<<4) //A4 
        #define SET_TP4 PORTC |= (1<<4) 
        #define CLR_TP4 PORTC &= ~(1<<4) 
    #elif defined(__arm__) && (defined(__MK20DX128__) || defined(__MK20DX256__))
        // Teensys 3.x
        #define MODE_TP1 pinMode( A1,OUTPUT )   // A1= PortC, Bit0
        #define SET_TP1  GPIOC_PSOR = 0x01
        #define CLR_TP1  GPIOC_PCOR = 0x01
        #define MODE_TP2 pinMode( A2,OUTPUT )   // A2= PortB Bit0
        #define SET_TP2  GPIOB_PSOR = 0x01
        #define CLR_TP2  GPIOB_PCOR = 0x01
        #define MODE_TP3 pinMode( A3,OUTPUT )   // A3 = PortB Bit1
        #define SET_TP3  GPIOB_PSOR = 0x02
        #define CLR_TP3  GPIOB_PCOR = 0x02
        #define MODE_TP4 pinMode( A4,OUTPUT )   // A4 = PortB Bit3
        #define SET_TP4  GPIOB_PSOR = 0x08
        #define CLR_TP4  GPIOB_PCOR = 0x08
    #elif defined (__STM32F1__)
        // STM32F103...
        #define MODE_TP1 pinMode( PB12,OUTPUT )   // TP1= PB12
        #define SET_TP1  gpio_write_bit( GPIOB,12, HIGH );
        #define CLR_TP1  gpio_write_bit( GPIOB,12, LOW );
        #define MODE_TP2 pinMode( PB13,OUTPUT )   // TP2= PB13
        #define SET_TP2  gpio_write_bit( GPIOB,13, HIGH );
        #define CLR_TP2  gpio_write_bit( GPIOB,13, LOW );
        #define MODE_TP3 pinMode( PB14,OUTPUT )   // TP3 = PB14
        #define SET_TP3  gpio_write_bit( GPIOB,14, HIGH );
        #define CLR_TP3  gpio_write_bit( GPIOB,14, LOW );
        #define MODE_TP4 pinMode( PB15,OUTPUT )   // TP4 = PB15
        #define SET_TP4  gpio_write_bit( GPIOB,15, HIGH );
        #define CLR_TP4  gpio_write_bit( GPIOB,15, LOW );
    #elif defined(ESP8266)
        #define MODE_TP1 pinMode( D5,OUTPUT ) ; // GPIO 14
        #define SET_TP1  GPOS = (1 << D5);
        #define CLR_TP1  GPOC = (1 << D5);
        #define MODE_TP2 pinMode( D6,OUTPUT ) ; // GPIO 12
        #define SET_TP2  GPOS = (1 << D6);
        #define CLR_TP2  GPOC = (1 << D6);
        #define MODE_TP3 pinMode( D7,OUTPUT ) ; // GPIO 13
        #define SET_TP3  GPOS = (1 << D7);
        #define CLR_TP3  GPOC = (1 << D7);
        #define MODE_TP4 pinMode( D8,OUTPUT ) ; // GPIO 15
        #define SET_TP4  GPOS = (1 << D8);
        #define CLR_TP4  GPOC = (1 << D8);
    #elif defined(ESP32)
        #define MODE_TP1 pinMode( 33,OUTPUT ) ; // GPIO 33
        #define SET_TP1  GPOS = (1 << 33);
        #define CLR_TP1  GPOC = (1 << 33);
        #define MODE_TP2 pinMode( 25,OUTPUT ) ; // GPIO 25
        #define SET_TP2  GPOS = (1 << 25);
        #define CLR_TP2  GPOC = (1 << 25);
        #define MODE_TP3 pinMode( 26,OUTPUT ) ; // GPIO 26
        #define SET_TP3  GPOS = (1 << 26);
        #define CLR_TP3  GPOC = (1 << 26);
        #define MODE_TP4 pinMode( 27,OUTPUT ) ; // GPIO 27
        #define SET_TP4  GPOS = (1 << 27);
        #define CLR_TP4  GPOC = (1 << 27);
        
        
    //#elif defined(__AVR_ATmega128__) ||defined(__AVR_ATmega1281__)||defined(__AVR_ATmega2561__)
    #elif defined(NODE_DEBUG)
        #define PULLUP PLATFORM_GPIO_PULLUP
        #define OUTPUT PLATFORM_GPIO_OUTPUT
        #define HIGH PLATFORM_GPIO_HIGH
        #define LOW PLATFORM_GPIO_LOW

        #define MODE_TP1 platform_gpio_mode( 5, OUTPUT, PULLUP ); // GPIO 14
        #define SET_TP1  platform_gpio_write(5, HIGH);
        #define CLR_TP1  platform_gpio_write(5, LOW);
        #define MODE_TP2 platform_gpio_mode( 6, OUTPUT, PULLUP ); // GPIO 12
        #define SET_TP2  platform_gpio_write(6, HIGH);
        #define CLR_TP2  platform_gpio_write(6, LOW);
        #define MODE_TP3 platform_gpio_mode( 7, OUTPUT, PULLUP ); // GPIO 13
        #define SET_TP3  platform_gpio_write(7, HIGH);
        #define CLR_TP3  platform_gpio_write(7, LOW);
        #define MODE_TP4 platform_gpio_mode( 8, OUTPUT, PULLUP ); // GPIO 15
        #define SET_TP4  platform_gpio_write(8, HIGH);
        #define CLR_TP4  platform_gpio_write(8, LOW);
    #else
        #define MODE_TP1 
        #define SET_TP1 
        #define CLR_TP1 
        #define MODE_TP2 
        #define SET_TP2 
        #define CLR_TP2 
        #define MODE_TP3 
        #define SET_TP3 
        #define CLR_TP3 
        #define MODE_TP4 
        #define SET_TP4 
        #define CLR_TP4 
    
    #endif 
#else
    #define MODE_TP1 
    #define SET_TP1 
    #define CLR_TP1 
    #define MODE_TP2 
    #define SET_TP2 
    #define CLR_TP2 
    #define MODE_TP3 
    #define SET_TP3 
    #define CLR_TP3 
    #define MODE_TP4 
    #define SET_TP4 
    #define CLR_TP4 
    
#endif
#ifdef DEBUG_PRINT
  #ifdef NODEMCUDCC
    #define DB_PRINT NODE_DBG
  #else
    #define DB_PRINT( x, ... ) { char dbgbuf[80]; sprintf_P( dbgbuf, (const char*) F( x ) , ##__VA_ARGS__ ) ; Serial.println( dbgbuf ); }
    #define DB_PRINT_( x, ... ) { char dbgbuf[80]; sprintf_P( dbgbuf, (const char*) F( x ) , ##__VA_ARGS__ ) ; Serial.print( dbgbuf ); }
  #endif
#else
    #define DB_PRINT( x, ... ) ;
    #define DB_PRINT_( x, ... ) ;
#endif

#ifdef DCC_DBGVAR
struct countOf_t countOf;
#endif

#if defined ( __STM32F1__ )
static ExtIntTriggerMode ISREdge;
#elif defined ( ESP32 )
static byte  ISREdge;   // Holder of the Next Edge we're looking for: RISING or FALLING
static byte  ISRWatch;  // Interrupt Handler Edge Filter 
#elif defined ( NODEMCUDCC )
static uint8_t  ISREdge;   // Holder of the Next Edge we're looking for: RISING or FALLING
static int16_t  bitMax, bitMin;
DCC_MSG Msg ;
#else
static byte  ISREdge;   // Holder of the Next Edge we're looking for: RISING or FALLING
static byte  ISRWatch;  // Interrupt Handler Edge Filter 
#endif
byte ISRLevel;          // expected Level at DCC input during ISR ( to detect glitches )
byte ISRChkMask;       // Flag if Level must be checked
static word  bitMax, bitMin;

typedef enum
{
  WAIT_PREAMBLE = 0,
  WAIT_START_BIT,
  #ifndef SYNC_ALWAYS
  WAIT_START_BIT_FULL,
  #endif
  WAIT_DATA,
  WAIT_END_BIT
} 
DccRxWaitState ;

typedef enum
{
  OPS_INS_RESERVED = 0,
  OPS_INS_VERIFY_BYTE,
  OPS_INS_BIT_MANIPULATION,
  OPS_INS_WRITE_BYTE
}
OpsInstructionType;

struct DccRx_t
{
  DccRxWaitState  State ;
  uint8_t         DataReady ;
  uint8_t         BitCount ;
  uint8_t         TempByte ;
  uint8_t         chkSum;
  DCC_MSG         PacketBuf;
  DCC_MSG         PacketCopy;
} 
DccRx ;

typedef struct
{
  uint8_t   Flags ;
  uint8_t   OpsModeAddressBaseCV ;
  uint8_t   inServiceMode ;
  long      LastServiceModeMillis ;
  uint8_t   PageRegister ;  // Used for Paged Operations in Service Mode Programming
  uint8_t   DuplicateCount ;
  DCC_MSG   LastMsg ;
#ifdef NODEMCUDCC
  uint8_t   IntPin;
  uint8_t   IntBitmask;
#else  
  uint8_t	ExtIntNum; 
  uint8_t	ExtIntPinNum;
  volatile uint8_t   *ExtIntPort;     // use port and bitmask to read input at AVR in ISR
  uint8_t   ExtIntMask;     // digitalRead is too slow on AVR
#endif
  int16_t   myDccAddress;	// Cached value of DCC Address from CVs
  uint8_t   inAccDecDCCAddrNextReceivedMode;
  uint8_t	cv29Value; 
#ifdef DCC_DEBUG
  uint8_t	IntCount;
  uint8_t	TickCount;
  uint8_t   NestedIrqCount;
#endif
} 
DCC_PROCESSOR_STATE ;

DCC_PROCESSOR_STATE DccProcState ;

#ifdef ESP32
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR ExternalInterruptHandler(void)
#elif defined(ESP8266)
void ICACHE_RAM_ATTR ExternalInterruptHandler(void)
#elif defined(NODEMCUDCC)
task_handle_t   DataReady_taskid;
static uint32_t ICACHE_RAM_ATTR InterruptHandler (uint32_t ret_gpio_status)
#else
void ExternalInterruptHandler(void)
#endif
{
    SET_TP3;

#ifdef NODEMCUDCC
  // This function really is running at interrupt level with everything
  // else masked off. It should take as little time as necessary.
  uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  if ((gpio_status & DccProcState.IntBitmask) == 0) { 
    return ret_gpio_status;
  }
  
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & DccProcState.IntBitmask);
  ret_gpio_status &= ~(DccProcState.IntBitmask);
#endif
  
#ifdef ESP32
//   switch (ISRWatch)
//   {
//     case RISING: if (digitalRead(DccProcState.ExtIntPinNum)) break; 
//     case FALLING: if (digitalRead(DccProcState.ExtIntPinNum)) return; break; 
//   }
	// First compare the edge we're looking for to the pin state 
	switch (ISRWatch)
	{
		case CHANGE:
			break;
				
		case RISING:
			if (digitalRead(DccProcState.ExtIntPinNum) != HIGH)
				return; 
			break;
				
		case FALLING:
			if (digitalRead(DccProcState.ExtIntPinNum) != LOW)
				return;
			break; 
	}
#endif
// Bit evaluation without Timer 0 ------------------------------
    uint8_t DccBitVal;
    static int8_t  bit1, bit2 ;
    static unsigned int  lastMicros = 0;
    #ifdef NODEMCUDCC
    static byte halfBit, preambleBitCount;
    #else
    static byte halfBit, DCC_IrqRunning, preambleBitCount;
    #endif
    unsigned int  actMicros, bitMicros;
    #ifdef ALLOW_NESTED_IRQ
    if ( DCC_IrqRunning ) {
        // nested DCC IRQ - obviously there are glitches
        // ignore this interrupt and increment glitchcounter
        CLR_TP3;
        #ifdef DCC_DEBUG
            DccProcState.NestedIrqCount++;
        #endif
        SET_TP3;
        return; //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> abort IRQ
    }
    #endif
#ifdef NODEMCUDCC
    actMicros = system_get_time();
#else
    actMicros = micros();
#endif
    bitMicros = actMicros-lastMicros;

        CLR_TP3; SET_TP3;
#ifdef __AVR_MEGA__
    if ( bitMicros < bitMin || ( DccRx.State != WAIT_START_BIT && (*DccProcState.ExtIntPort & DccProcState.ExtIntMask) != (ISRLevel) ) ) {
#elif defined(NODEMCUDCC)
    if ( bitMicros < bitMin ) {
#else
    if ( bitMicros < bitMin || ( DccRx.State != WAIT_START_BIT && digitalRead( DccProcState.ExtIntPinNum ) != (ISRLevel) ) ) {
#endif
        // too short - my be false interrupt due to glitch or false protocol  or level does not match RISING / FALLING edge -> ignore this IRQ
        CLR_TP3;
        SET_TP4; /*delayMicroseconds(1); */ CLR_TP4;
#ifdef NODEMCUDCC
        return ret_gpio_status; //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> abort IRQ
#else
        return; //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> abort IRQ
#endif
    }
        CLR_TP3;  SET_TP3;

    lastMicros = actMicros;
#ifndef SUPPORT_ZERO_BIT_STRETCHING
    //if ( bitMicros > MAX_ZEROBITFULL ) {
    if ( bitMicros > (bitMax*2) ) {
        // too long - my be false protocol -> start over
        DccRx.State = WAIT_PREAMBLE ;
        DccRx.BitCount = 0 ;
        preambleBitCount = 0;
        // SET_TP2; CLR_TP2;
        bitMax = MAX_PRAEAMBEL;
        bitMin = MIN_ONEBITFULL;
        #if defined ( __STM32F1__ )
        detachInterrupt( DccProcState.ExtIntNum );
        #endif
        #ifdef ESP32
        ISRWatch = ISREdge;
        #elif defined(NODEMCUDCC)
        gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[DccProcState.IntPin]), ISREdge );
        #else
        attachInterrupt( DccProcState.ExtIntNum, ExternalInterruptHandler, ISREdge );
        #endif
        // enable level-checking
        ISRChkMask = DccProcState.ExtIntMask;
        ISRLevel = (ISREdge==RISING)? DccProcState.ExtIntMask : 0 ;
        CLR_TP3;
        //CLR_TP3;
        return; //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> abort IRQ
    }
        CLR_TP3;
        SET_TP3;
#endif
    
    DccBitVal = ( bitMicros < bitMax );
    
    #ifdef ALLOW_NESTED_IRQ
    DCC_IrqRunning = true;
    interrupts();  // time critical is only the micros() command,so allow nested irq's
    #endif
    
#ifdef DCC_DEBUG
    DccProcState.TickCount++;
#endif
  switch( DccRx.State )
  {
  case WAIT_PREAMBLE:
    // We don't have to do anything special - looking for a preamble condition is done always
    SET_TP2;
    break;

#ifndef SYNC_ALWAYS
  case WAIT_START_BIT_FULL:
    // wait for startbit without level checking
    if ( !DccBitVal ) {
        // we got the startbit
        CLR_TP2;CLR_TP1;
        DccRx.State = WAIT_DATA ;
        CLR_TP1;
        // initialize packet buffer
        DccRx.PacketBuf.Size = 0;
        /*for(uint8_t i = 0; i< MAX_DCC_MESSAGE_LEN; i++ )
            DccRx.PacketBuf.Data[i] = 0;*/
        DccRx.PacketBuf.PreambleBits = preambleBitCount;
        DccRx.BitCount = 0 ;
        DccRx.chkSum = 0 ;
        DccRx.TempByte = 0 ;
        //SET_TP1;
    }
    break;
#endif    
  case WAIT_START_BIT:
    // we are looking for first half "0" bit after preamble
    switch ( halfBit ) {
      case 0: 
        // check first part
        if ( DccBitVal ) {
            // is still 1-bit (Preamble)
            halfBit=1;
            bit1=bitMicros;
        } else {
            // was "0" half bit, maybe the startbit
			halfBit = 4;
		}
        break;
      case 1: // previous halfbit was '1'
        if ( DccBitVal ) {
            // its a '1' halfBit -> we are still in the preamble
            halfBit = 0;
            bit2=bitMicros;
            preambleBitCount++;
            if( abs(bit2-bit1) > MAX_BITDIFF ) {
                // the length of the 2 halfbits differ too much -> wrong protokoll
                DccRx.State = WAIT_PREAMBLE;
                bitMax = MAX_PRAEAMBEL;
                bitMin = MIN_ONEBITFULL;
                preambleBitCount = 0;
                // SET_TP2; CLR_TP2;
                #if defined ( __STM32F1__ )
				detachInterrupt( DccProcState.ExtIntNum );
				#endif
                #ifdef ESP32
				ISRWatch = ISREdge;
                #elif defined(NODEMCUDCC)
                gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[DccProcState.IntPin]), ISREdge);
                #else
                attachInterrupt( DccProcState.ExtIntNum, ExternalInterruptHandler, ISREdge );
                // enable level checking ( with direct port reading @ AVR )
                ISRChkMask = DccProcState.ExtIntMask;       
                ISRLevel = (ISREdge==RISING)? DccProcState.ExtIntMask : 0 ;
                #endif
				SET_TP3;
                CLR_TP4;
            }
        } else {
            // first '0' half detected in second halfBit
            // wrong sync or not a DCC protokoll
            CLR_TP3;
			halfBit = 3;
            SET_TP3;
        }
        break;
      case 3: // previous halfbit was '0'  in second halfbit  
        if ( DccBitVal ) {
            // its a '1' halfbit -> we got only a half '0' bit -> cannot be DCC
            DccRx.State = WAIT_PREAMBLE;
            bitMax = MAX_PRAEAMBEL;
            bitMin = MIN_ONEBITFULL;
            preambleBitCount = 0;
            // SET_TP2; CLR_TP2;
        } else {
            // we got two '0' halfbits -> it's the startbit
            // but sync is NOT ok, change IRQ edge.
            CLR_TP2;CLR_TP1;
            if ( ISREdge == RISING ) ISREdge = FALLING; else ISREdge = RISING;
            DccRx.State = WAIT_DATA ;
            CLR_TP1;
            bitMax = MAX_ONEBITFULL;
            bitMin = MIN_ONEBITFULL;
            DccRx.PacketBuf.Size = 0;
            /*for(uint8_t i = 0; i< MAX_DCC_MESSAGE_LEN; i++ )
            DccRx.PacketBuf.Data[i] = 0;*/
            DccRx.PacketBuf.PreambleBits = preambleBitCount;
            DccRx.BitCount = 0 ;
            DccRx.chkSum = 0 ;
            DccRx.TempByte = 0 ;
            //SET_TP1;
        }
		//SET_TP4;

        #if defined ( __STM32F1__ )
        detachInterrupt( DccProcState.ExtIntNum );
        #endif
        #ifdef ESP32
        ISRWatch = ISREdge;
        #elif defined(NODEMCUDCC)
        gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[DccProcState.IntPin]), ISREdge);
        #else
        attachInterrupt( DccProcState.ExtIntNum, ExternalInterruptHandler, ISREdge );
        #endif
        #ifndef NODEMCUDCC
        // enable level-checking
        ISRChkMask = DccProcState.ExtIntMask;
        ISRLevel = (ISREdge==RISING)? DccProcState.ExtIntMask : 0 ;
        //CLR_TP4;
        #endif
        break;
      case 4: // previous (first) halfbit was 0
        // if this halfbit is 0 too, we got the startbit
        if ( DccBitVal ) {
            // second halfbit is 1 -> unknown protokoll
            DccRx.State = WAIT_PREAMBLE;
            bitMax = MAX_PRAEAMBEL;
            bitMin = MIN_ONEBITFULL;
            preambleBitCount = 0;
            CLR_TP2;CLR_TP1;
            DccRx.BitCount = 0;
        } else {
            // we got the startbit
            CLR_TP2;CLR_TP1;
            DccRx.State = WAIT_DATA ;
            CLR_TP1;
            bitMax = MAX_ONEBITFULL;
            bitMin = MIN_ONEBITFULL;
            // initialize packet buffer
            DccRx.PacketBuf.Size = 0;
            /*for(uint8_t i = 0; i< MAX_DCC_MESSAGE_LEN; i++ )
            DccRx.PacketBuf.Data[i] = 0;*/
            DccRx.PacketBuf.PreambleBits = preambleBitCount;
            DccRx.BitCount = 0 ;
            DccRx.chkSum = 0 ;
            DccRx.TempByte = 0 ;
            //SET_TP1;
        }
		
        //SET_TP4;

		#if defined ( __STM32F1__ )
		detachInterrupt( DccProcState.ExtIntNum );
		#endif
        #ifdef ESP32
        ISRWatch = ISREdge;
        #elif defined(NODEMCUDCC)
        gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[DccProcState.IntPin]), ISREdge);
        #else
		attachInterrupt( DccProcState.ExtIntNum, ExternalInterruptHandler, ISREdge );
        #endif
        #ifndef NODEMCUDCC
        // enable level-checking
        ISRChkMask = DccProcState.ExtIntMask;
        ISRLevel = (ISREdge==RISING)? DccProcState.ExtIntMask : 0 ;
		//CLR_TP4;
        #endif
        break;
            
    }        
    break;

  case WAIT_DATA:
    CLR_TP2;
    DccRx.BitCount++;
    DccRx.TempByte = ( DccRx.TempByte << 1 ) ;
    if( DccBitVal )
      DccRx.TempByte |= 1 ;

    if( DccRx.BitCount == 8 )
    {
      if( DccRx.PacketBuf.Size == MAX_DCC_MESSAGE_LEN ) // Packet is too long - abort
      {
        DccRx.State = WAIT_PREAMBLE ;
        bitMax = MAX_PRAEAMBEL;
        bitMin = MIN_ONEBITFULL;
        DccRx.BitCount = 0 ;
      }
      else
      {
        DccRx.State = WAIT_END_BIT ;
        DccRx.PacketBuf.Data[ DccRx.PacketBuf.Size++ ] = DccRx.TempByte ;
        DccRx.chkSum ^= DccRx.TempByte;
      }
    }
    break;

  case WAIT_END_BIT:
    SET_TP2;CLR_TP2;
    DccRx.BitCount++;
    if( DccBitVal ) { // End of packet?
      CLR_TP3; SET_TP4;
      DccRx.State = WAIT_PREAMBLE ;
      DccRx.BitCount = 0 ;
      bitMax = MAX_PRAEAMBEL;
      bitMin = MIN_ONEBITFULL;
      SET_TP1;
      if ( DccRx.chkSum == 0 ) { 
        // Packet is valid
        #ifdef ESP32
        portENTER_CRITICAL_ISR(&mux);
        #endif
        DccRx.PacketCopy = DccRx.PacketBuf ;
        DccRx.DataReady = 1 ;
        #ifdef ESP32
        portEXIT_CRITICAL_ISR(&mux);
        #elif defined(NODEMCUDCC)
        task_post_high(DataReady_taskid, (os_param_t) 0);
        #endif
        // SET_TP2; CLR_TP2;
        preambleBitCount = 0 ;
      } else {
        // Wrong checksum
        CLR_TP1;
        #ifdef DCC_DBGVAR
        DB_PRINT("Cerr");
        countOf.Err++;
        #endif
      }

      SET_TP3; CLR_TP4;
    } else  { // Get next Byte
      // KGW - Abort immediately if packet is too long.
      if( DccRx.PacketBuf.Size == MAX_DCC_MESSAGE_LEN ) // Packet is too long - abort
      {
        DccRx.State = WAIT_PREAMBLE ;
        bitMax = MAX_PRAEAMBEL;
        bitMin = MIN_ONEBITFULL;
        DccRx.BitCount = 0 ;
      }
      else
      {
        DccRx.State = WAIT_DATA ;

        DccRx.BitCount = 0 ;
        DccRx.TempByte = 0 ;
      }
    }
  }

  // unless we're already looking for the start bit 
  // we always search for a preamble ( ( 10 or more consecutive 1 bits )
  // if we found it within a packet, the packet decoding is aborted because
  // that much one bits cannot be valid in a packet.
  if ( DccRx.State != WAIT_START_BIT ) {
    if( DccBitVal )
    {
      preambleBitCount++;
      //SET_TP2;
      if( preambleBitCount > 10 ) {
        CLR_TP2;
#ifndef SYNC_ALWAYS
        if ( DccRx.chkSum == 0 ) { 
            // sync must be correct if chksum was ok, no need to check sync
            DccRx.State = WAIT_START_BIT_FULL;
        } else {
#endif
        DccRx.State = WAIT_START_BIT ;
            SET_TP2;
            // While waiting for the start bit, detect halfbit lengths. We will detect the correct
            // sync and detect whether we see a false (e.g. motorola) protocol

            #if defined ( __STM32F1__ )
            detachInterrupt( DccProcState.ExtIntNum );
            #endif
            #ifdef ESP32
            ISRWatch = CHANGE;
            #elif defined(NODEMCUDCC)
            gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[DccProcState.IntPin]), CHANGE);
            #else
            attachInterrupt( DccProcState.ExtIntNum, ExternalInterruptHandler, CHANGE);
            #endif
            ISRChkMask = 0;         // AVR level check is always true with this settings
            ISRLevel = 0;           // ( there cannot be false edge IRQ's with CHANGE )
            halfBit = 0;
            bitMax = MAX_ONEBITHALF;
            bitMin = MIN_ONEBITHALF;
            //CLR_TP1;
#ifndef SYNC_ALWAYS
        }
#endif
      }
    } else {
        CLR_TP1;
        preambleBitCount = 0 ;
        // SET_TP2; CLR_TP2;
    }
  }

  #ifdef ALLOW_NESTED_IRQ
  DCC_IrqRunning = false;
  #endif
  //CLR_TP1;
  CLR_TP3;
#ifdef NODEMCUDCC
  return ret_gpio_status;
#endif
}

void ackCV(void)
{
  if( notifyCVAck )
  {
    DB_PRINT("ackCV: Send Basic ACK");
    notifyCVAck() ;
  }
}

void ackAdvancedCV(void)
{
  if( notifyAdvancedCVAck && (DccProcState.cv29Value & CV29_RAILCOM_ENABLE) )
  {
    DB_PRINT("ackAdvancedCV: Send RailCom ACK");
    notifyAdvancedCVAck() ;
  }
}


#ifndef NODEMCUDCC
uint8_t readEEPROM( unsigned int CV )
{
    return EEPROM.read(CV) ;
}

void writeEEPROM( unsigned int CV, uint8_t Value )
{
    EEPROM.write(CV, Value) ;
  #if defined(ESP8266)
    EEPROM.commit();
  #endif
  #if defined(ESP32)
    EEPROM.commit();
  #endif
}

bool readyEEPROM()
{
#if defined ARDUINO_ARCH_MEGAAVR
	return bit_is_clear(NVMCTRL.STATUS,NVMCTRL_EEBUSY_bp);
#elif defined __AVR_MEGA__
	return eeprom_is_ready();
#else
	return true;
#endif
}
#endif

uint8_t validCV( uint16_t CV, uint8_t Writable )
{
  if( notifyCVResetFactoryDefault && (CV == CV_MANUFACTURER_ID )  && Writable )
	notifyCVResetFactoryDefault();
	
  if( notifyCVValid )
    return notifyCVValid( CV, Writable ) ;

#ifdef NODEMCUDCC
  return 0;
#else
  uint8_t Valid = 1 ;

  if( CV > MAXCV )
    Valid = 0 ;

  if( Writable && ( ( CV ==CV_VERSION_ID ) || (CV == CV_MANUFACTURER_ID ) ) )
    Valid = 0 ;

  return Valid ;
#endif
}

#ifdef NODEMCUDCC
uint16_t readCV( unsigned int CV )
#else
uint8_t readCV( unsigned int CV )
#endif
{
#ifndef NODEMCUDCC
  uint8_t Value ;
#endif

  if( notifyCVRead )
    return notifyCVRead( CV ) ;

#ifndef NODEMCUDCC
  Value = readEEPROM(CV);
  return Value ;
#else
  return 0;
#endif
}

uint8_t writeCV( unsigned int CV, uint8_t Value)
{
  switch( CV )
  {
    case CV_29_CONFIG:
      // copy addressmode Bit to Flags
      Value = Value &  ~CV29_RAILCOM_ENABLE;   // Bidi (RailCom) Bit must not be enabled, 
                                            // because you cannot build a Bidi decoder with this lib.
      DccProcState.cv29Value = Value;
      DccProcState.Flags = ( DccProcState.Flags & ~FLAGS_CV29_BITS) | (Value & FLAGS_CV29_BITS);
      // no break, because myDccAdress must also be reset
    case CV_ACCESSORY_DECODER_ADDRESS_LSB:	// Also same CV for CV_MULTIFUNCTION_PRIMARY_ADDRESS
    case CV_ACCESSORY_DECODER_ADDRESS_MSB:
    case CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB:
    case CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB:
	  DccProcState.myDccAddress = -1;	// Assume any CV Write Operation might change the Address
  }
  
  if( notifyCVWrite )
    return notifyCVWrite( CV, Value ) ;

#ifdef NODEMCUDCC
  return 0;
#else
  if( readEEPROM( CV ) != Value )
  {
    writeEEPROM( CV, Value ) ;

    if( notifyCVChange )
      notifyCVChange( CV, Value) ;

    if( notifyDccCVChange && !(DccProcState.Flags & FLAGS_SETCV_CALLED) )
      notifyDccCVChange( CV, Value );
  }

  return readEEPROM( CV ) ;
#endif
}

uint16_t getMyAddr(void)
{
  if( DccProcState.myDccAddress != -1 )	// See if we can return the cached value 
  	return( DccProcState.myDccAddress );

  if( DccProcState.cv29Value & CV29_ACCESSORY_DECODER )  // Accessory Decoder?
  {
    if( DccProcState.cv29Value & CV29_OUTPUT_ADDRESS_MODE ) 
      DccProcState.myDccAddress = ( readCV( CV_ACCESSORY_DECODER_ADDRESS_MSB ) << 8 ) | readCV( CV_ACCESSORY_DECODER_ADDRESS_LSB );
    else
      DccProcState.myDccAddress = ( ( readCV( CV_ACCESSORY_DECODER_ADDRESS_MSB ) & 0b00000111) << 6 ) | ( readCV( CV_ACCESSORY_DECODER_ADDRESS_LSB ) & 0b00111111) ;
  }
  else   // Multi-Function Decoder?
  {
    if( DccProcState.cv29Value & CV29_EXT_ADDRESSING )  // Two Byte Address?
      DccProcState.myDccAddress = ( ( readCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB ) - 192 ) << 8 ) | readCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB ) ;

    else
      DccProcState.myDccAddress = readCV( 1 ) ;
  }
  	
  return DccProcState.myDccAddress ;
}

void processDirectCVOperation( uint8_t Cmd, uint16_t CVAddr, uint8_t Value, void (*ackFunction)() )
{
  // is it a Byte Operation
  if( Cmd & 0x04 )
  {
    // Perform the Write Operation
    if( Cmd & 0x08 )
    {
      if( validCV( CVAddr, 1 ) )
      {
        DB_PRINT("CV: %d Byte Write: %02X", CVAddr, Value)
        if( writeCV( CVAddr, Value ) == Value )
          ackFunction();
      }
    }

    else  // Perform the Verify Operation
    {  
      if( validCV( CVAddr, 0 ) )
      {
        DB_PRINT("CV: %d Byte Read: %02X", CVAddr, Value)
        if( readCV( CVAddr ) == Value )
          ackFunction();
      }
    }
  }
  // Perform the Bit-Wise Operation
  else
  {
    uint8_t BitMask = (1 << (Value & 0x07) ) ;
    uint8_t BitValue = Value & 0x08 ;
    uint8_t BitWrite = Value & 0x10 ;

#ifdef NODEMCUDCC
    uint16_t tempValue = readCV( CVAddr ) ;  // Read the Current CV Value
#else
    uint8_t tempValue = readCV( CVAddr ) ;  // Read the Current CV Value
#endif

#ifdef NODEMCUDCC
    if (tempValue <= 255) {
        DB_PRINT("CV: %d Current Value: %02X  Bit-Wise Mode: %s  Mask: %02X  Value: %02X", CVAddr, tempValue, BitWrite ? "Write":"Read", BitMask, BitValue);
#else
    DB_PRINT("CV: %d Current Value: %02X  Bit-Wise Mode: %s  Mask: %02X  Value: %02X", CVAddr, tempValue, BitWrite ? "Write":"Read", BitMask, BitValue);
#endif

    // Perform the Bit Write Operation
    if( BitWrite )
    {
      if( validCV( CVAddr, 1 ) )
      {
        if( BitValue )
          tempValue |= BitMask ;     // Turn the Bit On

        else
          tempValue &= ~BitMask ;  // Turn the Bit Off

        if( writeCV( CVAddr, tempValue ) == tempValue )
          ackFunction() ;
      }
    }
    // Perform the Bit Verify Operation
    else
    {
      if( validCV( CVAddr, 0 ) )
      {
        if( BitValue ) 
        {
          if( tempValue & BitMask )
            ackFunction() ;
        }
        else
        {
          if( !( tempValue & BitMask)  )
            ackFunction() ;
        }
      }
    }
#ifdef NODEMCUDCC
    }
#endif
  }
}

/////////////////////////////////////////////////////////////////////////
#ifdef NMRA_DCC_PROCESS_MULTIFUNCTION
void processMultiFunctionMessage( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Cmd, uint8_t Data1, uint8_t Data2 )
{
  uint8_t  speed ;
  uint16_t CVAddr ;
  DCC_DIRECTION dir ;
  DCC_SPEED_STEPS speedSteps ;

  uint8_t  CmdMasked = Cmd & 0b11100000 ;

  // NODE_DBG("[dcc_processMultiFunctionMessage] Addr: %d, Type: %d, Cmd: %d ("BYTE_TO_BINARY_PATTERN"), Data: %d, %d, CmdMasked="BYTE_TO_BINARY_PATTERN"\n", Addr, AddrType, Cmd, BYTE_TO_BINARY(Cmd), Data1, Data2, BYTE_TO_BINARY(CmdMasked)); 

  // If we are an Accessory Decoder
  if( DccProcState.Flags & FLAGS_DCC_ACCESSORY_DECODER )
  {
    // NODE_DBG("[dcc_processMultiFunctionMessage] DccProcState.Flags & FLAGS_DCC_ACCESSORY_DECODER\n");
    
    // and this isn't an Ops Mode Write or we are NOT faking the Multifunction Ops mode address in CV 33+34 or
    // it's not our fake address, then return
    if( ( CmdMasked != 0b11100000 ) || ( DccProcState.OpsModeAddressBaseCV == 0 ) )
      return ;

    uint16_t FakeOpsAddr = readCV( DccProcState.OpsModeAddressBaseCV ) | ( readCV( DccProcState.OpsModeAddressBaseCV + 1 ) << 8 ) ;
    uint16_t OpsAddr = Addr & 0x3FFF ;

    if( OpsAddr != FakeOpsAddr )
      return ;
  }

  // We are looking for FLAGS_MY_ADDRESS_ONLY but it does not match and it is not a Broadcast Address then return
  else if( ( DccProcState.Flags & FLAGS_MY_ADDRESS_ONLY ) && ( Addr != getMyAddr() ) && ( Addr != 0 ) ) 
    return ;

  NODE_DBG("[dcc_processMultiFunctionMessage] CmdMasked: %x\n", CmdMasked);
  switch( CmdMasked )
  {
  case 0b00000000:  // Decoder Control
    switch( Cmd & 0b00001110 )
    {
    case 0b00000000:  
      if( notifyDccReset)
        notifyDccReset( Cmd & 0b00000001 ) ;
      break ;

    case 0b00000010:  // Factory Test
      break ;

    case 0b00000110:  // Set Decoder Flags
      break ;

    case 0b00001010:  // Set Advanced Addressing
      break ;

    case 0b00001110:  // Decoder Acknowledgment
      break ;

    default:    // Reserved
      ;
    }
    break ;

  case 0b00100000:  // Advanced Operations
    switch( Cmd & 0b00011111 )
    {
    case 0b00011111:
      if( notifyDccSpeed )
      {
        switch( Data1 & 0b01111111 )
        {
        case 0b00000000:  // 0=STOP
          speed = 1 ;     // => 1
          break ;

        case 0b00000001:  // 1=EMERGENCY_STOP
          speed = 0 ;     // => 0
          break ;

        default:          // 2..127
          speed = (Data1 & 0b01111111) ;
        }
        dir = (DCC_DIRECTION) ((Data1 & 0b10000000) >> 7) ;
        notifyDccSpeed( Addr, AddrType, speed, dir, SPEED_STEP_128 ) ;
      }
    }
    break;

  case 0b01000000:
  case 0b01100000:
    //TODO should we cache this info in DCC_PROCESSOR_STATE.Flags ?
#ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
    speedSteps = (DccProcState.cv29Value & CV29_F0_LOCATION) ? SPEED_STEP_28 : SPEED_STEP_14 ;
#else
    speedSteps = SPEED_STEP_28 ;
#endif    
    if( notifyDccSpeed )
    {
      switch( Cmd & 0b00011111 )
      {
      case 0b00000000:    // 0 0000 = STOP   
      case 0b00010000:    // 1 0000 = STOP
        speed = 1 ;       // => 1
        break ;

      case 0b00000001:    // 0 0001 = EMERGENCY STOP
      case 0b00010001:    // 1 0001 = EMERGENCY STOP
        speed = 0 ;       // => 0
        break ;

      default:
#ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
        if( speedSteps == SPEED_STEP_14 )
        {
          speed = (Cmd & 0b00001111) ; // => 2..15
        }
        else
        {
#endif
          speed = (((Cmd & 0b00001111) << 1 ) | ((Cmd & 0b00010000) >> 4)) - 2 ; // => 2..29
#ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
        }    
#endif        
      }
      dir = (DCC_DIRECTION) ((Cmd & 0b00100000) >> 5) ;
      notifyDccSpeed( Addr, AddrType, speed, dir, speedSteps ) ;
    }
    if( notifyDccSpeedRaw )
    	notifyDccSpeedRaw(Addr, AddrType, Cmd );

#ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
    if( notifyDccFunc && (speedSteps == SPEED_STEP_14) )
    {
      // function light is controlled by this package
      uint8_t fn0 = (Cmd & 0b00010000) ;
      notifyDccFunc( Addr, AddrType, FN_0, fn0 ) ;
    }
#endif
    break;

  case 0b10000000:  // Function Group 0..4
    if( notifyDccFunc )
    { 
        // function light is controlled by this package (28 or 128 speed steps)
        notifyDccFunc( Addr, AddrType, FN_0_4, Cmd & 0b00011111 ) ;
    }
    break;

  case 0b10100000:  // Function Group 5..8
    if( notifyDccFunc)
    {
      if (Cmd & 0b00010000 )
        notifyDccFunc( Addr, AddrType, FN_5_8,  Cmd & 0b00001111 ) ;
      else
        notifyDccFunc( Addr, AddrType, FN_9_12, Cmd & 0b00001111 ) ;
    }
    break;

  case 0b11000000:  // Feature Expansion Instruction
  	switch(Cmd & 0b00011111)
  	{
  	case 0b00011110:
  	  if( notifyDccFunc )
	    notifyDccFunc( Addr, AddrType, FN_13_20, Data1 ) ;
	  break;
	  
  	case 0b00011111:
  	  if( notifyDccFunc )
	    notifyDccFunc( Addr, AddrType, FN_21_28, Data1 ) ;
	  break;
  	}
    break;

  case 0b11100000:  // CV Access
    CVAddr = ( ( ( Cmd & 0x03 ) << 8 ) | Data1 ) + 1 ;

    processDirectCVOperation( Cmd, CVAddr, Data2, ackAdvancedCV) ;
    break;
  }
}
#endif

/////////////////////////////////////////////////////////////////////////
#ifdef NMRA_DCC_PROCESS_SERVICEMODE
void processServiceModeOperation( DCC_MSG * pDccMsg )
{
  uint16_t CVAddr ;
  uint8_t Value ;
  if( pDccMsg->Size == 3) // 3 Byte Packets are for Address Only, Register and Paged Mode
  {
    uint8_t RegisterAddr ;
    DB_PRINT("CV Address, Register & Paged Mode Operation");
    RegisterAddr = pDccMsg->Data[0] & 0x07 ;
    Value = pDccMsg->Data[1] ;

    if( RegisterAddr == 5 )
    {
      DccProcState.PageRegister = Value ;
      ackCV();
    }

    else
    {
      if( RegisterAddr == 4 )
        CVAddr = CV_29_CONFIG ;

      else if( ( RegisterAddr <= 3 ) && ( DccProcState.PageRegister > 0 ) )
        CVAddr = ( ( DccProcState.PageRegister - 1 ) * 4 ) + RegisterAddr + 1 ;

      else
        CVAddr = RegisterAddr + 1 ;

      if( pDccMsg->Data[0] & 0x08 ) // Perform the Write Operation
      {
        if( validCV( CVAddr, 1 ) )
        {
          if( writeCV( CVAddr, Value ) == Value )
            ackCV();
        }
      }

      else  // Perform the Verify Operation
      {  
        if( validCV( CVAddr, 0 ) )
        {
          if( readCV( CVAddr ) == Value )
            ackCV();
        }
      }
    }
  }

  else if( pDccMsg->Size == 4) // 4 Byte Packets are for Direct Byte & Bit Mode
  {
    DB_PRINT("CV Direct Byte and Bit Mode Mode Operation");
    CVAddr = ( ( ( pDccMsg->Data[0] & 0x03 ) << 8 ) | pDccMsg->Data[1] ) + 1 ;
    Value = pDccMsg->Data[2] ;
    
    processDirectCVOperation( pDccMsg->Data[0] & 0b00001100, CVAddr, Value, ackCV) ;
  }
}
#endif

/////////////////////////////////////////////////////////////////////////
void resetServiceModeTimer(uint8_t inServiceMode)
{
  if (notifyServiceMode && inServiceMode != DccProcState.inServiceMode)
  {
    notifyServiceMode(inServiceMode);
  }
  // Set the Service Mode
  DccProcState.inServiceMode = inServiceMode ;
  
  DccProcState.LastServiceModeMillis = inServiceMode ? millis() : 0 ;
  if (notifyServiceMode && inServiceMode != DccProcState.inServiceMode)
  {
    notifyServiceMode(inServiceMode);
  }
}

/////////////////////////////////////////////////////////////////////////
void clearDccProcState(uint8_t inServiceMode)
{
  resetServiceModeTimer( inServiceMode ) ;

  // Set the Page Register to it's default of 1 only on the first Reset
  DccProcState.PageRegister = 1 ;

  // Clear the LastMsg buffer and DuplicateCount in preparation for possible CV programming
  DccProcState.DuplicateCount = 0 ;
  memset( &DccProcState.LastMsg, 0, sizeof( DCC_MSG ) ) ;
}

/////////////////////////////////////////////////////////////////////////
#ifdef DEBUG_PRINT
void SerialPrintPacketHex(const __FlashStringHelper *strLabel, DCC_MSG * pDccMsg)
{
  Serial.print( strLabel );
 
  for( uint8_t i = 0; i < pDccMsg->Size; i++ )
  {
  	if( pDccMsg->Data[i] <= 9)
	  Serial.print('0');
  	
	Serial.print( pDccMsg->Data[i], HEX );
	Serial.write( ' ' );
  }
  Serial.println();
}
#endif

///////////////////////////////////////////////////////////////////////////////
void execDccProcessor( DCC_MSG * pDccMsg )
{
  NODE_DBG("[dcc_execDccProcessor]\n"); 
  if( ( pDccMsg->Data[0] == 0 ) && ( pDccMsg->Data[1] == 0 ) )
  {
    if( notifyDccReset )
      notifyDccReset( 0 ) ;

#ifdef NMRA_DCC_PROCESS_SERVICEMODE
    // If this is the first Reset then perform some one-shot actions as we maybe about to enter service mode
    if( DccProcState.inServiceMode )
      resetServiceModeTimer( 1 ) ;
    else
      clearDccProcState( 1 );
#endif
  }

  else
  {
#ifdef NMRA_DCC_PROCESS_SERVICEMODE
    if( DccProcState.inServiceMode && ( pDccMsg->Data[0] >= 112 ) && ( pDccMsg->Data[0] < 128 ) )
    {
      resetServiceModeTimer( 1 ) ;

	//Only check the DCC Packet "Size" and "Data" fields and ignore the "PreambleBits" as they can be different to the previous packet
      if(pDccMsg->Size != DccProcState.LastMsg.Size || memcmp( pDccMsg->Data, &DccProcState.LastMsg.Data, pDccMsg->Size ) != 0 )	      
      {
        DccProcState.DuplicateCount = 0 ;
        memcpy( &DccProcState.LastMsg, pDccMsg, sizeof( DCC_MSG ) ) ;
      }
      // Wait until you see 2 identical packets before acting on a Service Mode Packet 
      else
      {
        DccProcState.DuplicateCount++ ;
        processServiceModeOperation( pDccMsg ) ;
      }
    }

    else
    {
      if( DccProcState.inServiceMode )
        clearDccProcState( 0 );	
#endif

      // Idle Packet
      if( ( pDccMsg->Data[0] == 0b11111111 ) && ( pDccMsg->Data[1] == 0 ) )
      {
        if( notifyDccIdle )
          notifyDccIdle() ;
      }

#ifdef NMRA_DCC_PROCESS_MULTIFUNCTION
      // Multi Function Decoders (7-bit address)
      else if( pDccMsg->Data[0] < 128 )
        processMultiFunctionMessage( pDccMsg->Data[0], DCC_ADDR_SHORT, pDccMsg->Data[1], pDccMsg->Data[2], pDccMsg->Data[3] ) ;  

      // Basic Accessory Decoders (9-bit) & Extended Accessory Decoders (11-bit)
      else if( pDccMsg->Data[0] < 192 )
#else
      else if( ( pDccMsg->Data[0] >= 128 ) && ( pDccMsg->Data[0] < 192 ) )
#endif
      {
        if( DccProcState.Flags & FLAGS_DCC_ACCESSORY_DECODER )
        {
          int16_t BoardAddress ;
          int16_t OutputAddress ;
          uint8_t TurnoutPairIndex ;
          
#ifdef DEBUG_PRINT
          SerialPrintPacketHex(F( "eDP: AccCmd: "), pDccMsg);
#endif          

          BoardAddress = ( ( (~pDccMsg->Data[1]) & 0b01110000 ) << 2 ) | ( pDccMsg->Data[0] & 0b00111111 ) ;
          TurnoutPairIndex = (pDccMsg->Data[1] & 0b00000110) >> 1;
          DB_PRINT("[dcc_execDccProcessor] eDP: BAddr:%d, Index:%d", BoardAddress, TurnoutPairIndex);
          
          // First check for Legacy Accessory Decoder Configuration Variable Access Instruction
          // as it's got a different format to the others 
          if((pDccMsg->Size == 5) && ((pDccMsg->Data[1] & 0b10001100) == 0b00001100))
          {
            DB_PRINT( "eDP: Legacy Accessory Decoder CV Access Command");
            // Check if this command is for our address or the broadcast address
            if((BoardAddress != getMyAddr()) && ( BoardAddress < 511 )) 
            {
              DB_PRINT("[dcc_execDccProcessor] eDP: Board Address Not Matched");
              return;
            }

            uint16_t cvAddress = ((pDccMsg->Data[1] & 0b00000011) << 8) + pDccMsg->Data[2] + 1;
		  	uint8_t  cvValue   = pDccMsg->Data[3];
            DB_PRINT("[dcc_execDccProcessor] eDP: CV:%d Value:%d", cvAddress, cvValue );
          	if(validCV( cvAddress, 1 ))
              writeCV(cvAddress, cvValue);
          	return;
          }


          OutputAddress = (((BoardAddress - 1) << 2 ) | TurnoutPairIndex) + 1 ; //decoder output addresses start with 1, packet address range starts with 0
                                                                                // ( according to NMRA 9.2.2 )
          DB_PRINT("[dcc_execDccProcessor] eDP: OAddr:%d", OutputAddress);
          
          if( DccProcState.inAccDecDCCAddrNextReceivedMode)
          {
          	if( DccProcState.Flags & FLAGS_OUTPUT_ADDRESS_MODE )
          	{
              DB_PRINT("eDP: Set OAddr:%d", OutputAddress);
			  //uint16_t storedOutputAddress = OutputAddress + 1; // The value stored in CV1 & 9 for Output Addressing Mode is + 1
          	  writeCV(CV_ACCESSORY_DECODER_ADDRESS_LSB, (uint8_t)(OutputAddress % 256));
          	  writeCV(CV_ACCESSORY_DECODER_ADDRESS_MSB, (uint8_t)(OutputAddress / 256));
          	  
          	  if( notifyDccAccOutputAddrSet )
          	  	notifyDccAccOutputAddrSet(OutputAddress);
          	} 
          	else
          	{
              DB_PRINT("eDP: Set BAddr:%d", BoardAddress);
          	  writeCV(CV_ACCESSORY_DECODER_ADDRESS_LSB, (uint8_t)(BoardAddress % 64));
          	  writeCV(CV_ACCESSORY_DECODER_ADDRESS_MSB, (uint8_t)(BoardAddress / 64));
          	  
          	  if( notifyDccAccBoardAddrSet )
          	  	notifyDccAccBoardAddrSet(BoardAddress);
          	}
          	
          	DccProcState.inAccDecDCCAddrNextReceivedMode = 0; // Reset the mode now that we have set the address 
          }

          // If we're filtering addresses, does the address match our address or is it a broadcast address? If NOT then return
          if( DccProcState.Flags & FLAGS_MY_ADDRESS_ONLY )
          {
            if( DccProcState.Flags & FLAGS_OUTPUT_ADDRESS_MODE ) {
              DB_PRINT("[dcc_execDccProcessor] AddrChk: OAddr:%d, BAddr:%d, myAddr:%d Chk=%d", OutputAddress, BoardAddress, getMyAddr(), OutputAddress != getMyAddr() );
              if ( OutputAddress != getMyAddr()  &&  OutputAddress < 2045 ) {
                DB_PRINT("[dcc_execDccProcessor] eDP: OAddr:%d, myAddr:%d - no match", OutputAddress, getMyAddr() );
                return;
              }  
            } else {
              if( ( BoardAddress != getMyAddr() ) && ( BoardAddress < 511 ) ) {
                DB_PRINT("[dcc_execDccProcessor] eDP: BAddr:%d, myAddr:%d - no match", BoardAddress, getMyAddr() );
                return;
              }
            }
	        DB_PRINT("[dcc_execDccProcessor] eDP: Address Matched");
          }
          

		  if((pDccMsg->Size == 4) && ((pDccMsg->Data[1] & 0b10001001) == 1))	// Extended Accessory Decoder Control Packet Format
		  {
		  	// According to the NMRA Dcc Spec the Signal State should only use the lower 5 Bits,  
		  	// however some manufacturers seem to allow/use all 8 bits, so we'll relax that constraint for now
          	uint8_t state = pDccMsg->Data[2] ;
            DB_PRINT("eDP: OAddr:%d  Extended State:%0X", OutputAddress, state);
            if( notifyDccSigOutputState )
              notifyDccSigOutputState(OutputAddress, state);
              
            // old callback ( for compatibility with 1.4.2, not to be used in new designs )
            if( notifyDccSigState )
              notifyDccSigState( OutputAddress, TurnoutPairIndex, pDccMsg->Data[2] ) ;
		  }
		  
		  else if(pDccMsg->Size == 3)  // Basic Accessory Decoder Packet Format
		  {
          	uint8_t direction   =  pDccMsg->Data[1] & 0b00000001;
          	uint8_t outputPower = (pDccMsg->Data[1] & 0b00001000) >> 3;
            
            // old callback ( for compatibility with 1.4.2, not to be used in new designs )
            if ( notifyDccAccState )
              notifyDccAccState( OutputAddress, BoardAddress, pDccMsg->Data[1] & 0b00000111, outputPower );
          	
            if( DccProcState.Flags & FLAGS_OUTPUT_ADDRESS_MODE )
            {
              DB_PRINT("eDP: OAddr:%d  Turnout Dir:%d  Output Power:%d", OutputAddress, direction, outputPower);
              if( notifyDccAccTurnoutOutput )
                notifyDccAccTurnoutOutput( OutputAddress, direction, outputPower );
            }
            else
            {
              DB_PRINT("eDP: Turnout Pair Index:%d Dir:%d Output Power: ", TurnoutPairIndex, direction, outputPower);
              if( notifyDccAccTurnoutBoard )
            	notifyDccAccTurnoutBoard( BoardAddress, TurnoutPairIndex, direction, outputPower );
            }
          }
		  else if(pDccMsg->Size == 6) // Accessory Decoder OPS Mode Programming
		  {
            DB_PRINT("eDP: OPS Mode CV Programming Command");
              // Check for unsupported OPS Mode Addressing mode
		  	if(((pDccMsg->Data[1] & 0b10001001) != 1) && ((pDccMsg->Data[1] & 0b10001111) != 0x80))
		  	{
              DB_PRINT("eDP: Unsupported OPS Mode CV Addressing Mode");
              return;
            }
            
              // Check if this command is for our address or the broadcast address
            if(DccProcState.Flags & FLAGS_OUTPUT_ADDRESS_MODE)
            {
              DB_PRINT("eDP: Check Output Address:%d", OutputAddress);
              if((OutputAddress != getMyAddr()) && ( OutputAddress < 2045 ))
              {
                DB_PRINT("eDP: Output Address Not Matched");
              	return;
              }
            }
            else
            {
              DB_PRINT("eDP: Check Board Address:%d", BoardAddress);
              if((BoardAddress != getMyAddr()) && ( BoardAddress < 511 ))
              {
                DB_PRINT("eDP: Board Address Not Matched");
              	return;
              }
            }
              
		  	uint16_t cvAddress = ((pDccMsg->Data[2] & 0b00000011) << 8) + pDccMsg->Data[3] + 1;
		  	uint8_t  cvValue   = pDccMsg->Data[4];

		  	OpsInstructionType insType = (OpsInstructionType)((pDccMsg->Data[2] & 0b00001100) >> 2) ;

            DB_PRINT("eDP: OPS Mode Instruction:%d", insType);
			switch(insType)
			{
			case OPS_INS_RESERVED:
			case OPS_INS_VERIFY_BYTE:
              DB_PRINT("eDP: Unsupported OPS Mode Instruction:%d", insType);
		      break; // We only support Write Byte or Bit Manipulation
			
			case OPS_INS_WRITE_BYTE:
                DB_PRINT("eDP: CV:%d Value:%d", cvAddress, cvValue);
				if(validCV( cvAddress, 1 ))
                  writeCV(cvAddress, cvValue);
				break;
				
               // 111CDBBB
               // Where BBB represents the bit position within the CV,
               // D contains the value of the bit to be verified or written,
               // and C describes whether the operation is a verify bit or a write bit operation.
               // C = "1" WRITE BIT
               // C = "0" VERIFY BIT
			case OPS_INS_BIT_MANIPULATION:
				  // Make sure its a Write Bit Manipulation
				if((cvValue & 0b00010000) && validCV(cvAddress, 1 ))
				{
				  uint8_t currentValue = readCV(cvAddress);
				  uint8_t newValueMask = 1 << (cvValue & 0b00000111);
				  if(cvValue & 0b00001000)
				  	writeCV(cvAddress, currentValue | newValueMask);
				  else
				  	writeCV(cvAddress, currentValue & ~newValueMask);
				}
				break;
			}
          }
        }
      }

#ifdef NMRA_DCC_PROCESS_MULTIFUNCTION
      // Multi Function Decoders (14-bit address)
      else if( pDccMsg->Data[0] < 232 )
      {
        uint16_t Address ;
        Address = ( ( pDccMsg->Data[0] - 192 ) << 8 ) | pDccMsg->Data[1];
        //TODO should we convert Address to 1 .. 10239 ?
        processMultiFunctionMessage( Address, DCC_ADDR_LONG, pDccMsg->Data[2], pDccMsg->Data[3], pDccMsg->Data[4] ) ;  
      }
#endif
#ifdef NMRA_DCC_PROCESS_SERVICEMODE
    }
#endif
  }
}

////////////////////////////////////////////////////////////////////////
#ifndef NODEMCUDCC
NmraDcc::NmraDcc()
{
}

#ifdef digitalPinToInterrupt
void NmraDcc::pin( uint8_t ExtIntPinNum, uint8_t EnablePullup)
{
  pin(digitalPinToInterrupt(ExtIntPinNum), ExtIntPinNum, EnablePullup);
}
#endif

void NmraDcc::pin( uint8_t ExtIntNum, uint8_t ExtIntPinNum, uint8_t EnablePullup)
{
#if defined ( __STM32F1__ )
  // with STM32F1 the interuptnumber is equal the pin number
  DccProcState.ExtIntNum = ExtIntPinNum;
  // because STM32F1 has a NVIC we must set interuptpriorities
  const nvic_irq_num irqNum2nvic[] = { NVIC_EXTI0, NVIC_EXTI1, NVIC_EXTI2, NVIC_EXTI3, NVIC_EXTI4, 
            NVIC_EXTI_9_5, NVIC_EXTI_9_5, NVIC_EXTI_9_5, NVIC_EXTI_9_5, NVIC_EXTI_9_5, 
            NVIC_EXTI_15_10, NVIC_EXTI_15_10, NVIC_EXTI_15_10, NVIC_EXTI_15_10, NVIC_EXTI_15_10, NVIC_EXTI_15_10 };
  exti_num irqNum =  (exti_num)(PIN_MAP[ExtIntPinNum].gpio_bit); 

// DCC-Input IRQ must be able to interrupt other long low priority ( level15 ) IRQ's  
  nvic_irq_set_priority ( irqNum2nvic[irqNum], PRIO_DCC_IRQ); 
  
// Systic must be able to interrupt DCC-IRQ to always get correct micros() values  
  nvic_irq_set_priority(NVIC_SYSTICK, PRIO_SYSTIC); 
#else
  DccProcState.ExtIntNum = ExtIntNum;
#endif
  DccProcState.ExtIntPinNum = ExtIntPinNum;
#ifdef __AVR_MEGA__
    // because digitalRead at AVR is slow, we will read the dcc input in the ISR
    // by direct port access.
    DccProcState.ExtIntPort = portInputRegister( digitalPinToPort(ExtIntPinNum) );
    DccProcState.ExtIntMask = digitalPinToBitMask( ExtIntPinNum );
#else
    DccProcState.ExtIntMask = 1;
#endif	
  pinMode( ExtIntPinNum, EnablePullup ? INPUT_PULLUP : INPUT );
}

////////////////////////////////////////////////////////////////////////
void NmraDcc::initAccessoryDecoder( uint8_t ManufacturerId, uint8_t VersionId, uint8_t Flags, uint8_t OpsModeAddressBaseCV )
{
	init(ManufacturerId, VersionId, Flags | FLAGS_DCC_ACCESSORY_DECODER, OpsModeAddressBaseCV);
}
#endif //#ifndef NODEMCUDCC

////////////////////////////////////////////////////////////////////////
#ifdef NODEMCUDCC
void dcc_setup(uint8_t pin, uint8_t ManufacturerId, uint8_t VersionId, uint8_t Flags, uint8_t OpsModeAddressBaseCV)
#else
void NmraDcc::init( uint8_t ManufacturerId, uint8_t VersionId, uint8_t Flags, uint8_t OpsModeAddressBaseCV )
#endif
{
  #if defined(ESP8266)
    EEPROM.begin(MAXCV);
  #endif
  #if defined(ESP32)
    EEPROM.begin(MAXCV);
  #endif
  // Clear all the static member variables
  memset( &DccRx, 0, sizeof( DccRx) );

  MODE_TP1; // only for debugging and timing measurement
  MODE_TP2;
  MODE_TP3;
  MODE_TP4;
  bitMax = MAX_ONEBITFULL;
  bitMin = MIN_ONEBITFULL;

  DccProcState.Flags = Flags ;
  DccProcState.OpsModeAddressBaseCV = OpsModeAddressBaseCV ;
  DccProcState.myDccAddress = -1;
  DccProcState.inAccDecDCCAddrNextReceivedMode = 0;

  ISREdge = RISING;
#ifdef NODEMCUDCC
  DccProcState.IntPin = pin;
  DccProcState.IntBitmask = 1 << pin_num[pin];
#else
  // level checking to detect false IRQ's fired by glitches
  ISRLevel = DccProcState.ExtIntMask;
  ISRChkMask = DccProcState.ExtIntMask;
#endif

  #ifdef ESP32
  ISRWatch = ISREdge;
  attachInterrupt( DccProcState.ExtIntNum, ExternalInterruptHandler, CHANGE);
  #elif defined(NODEMCUDCC)
  platform_gpio_mode(pin, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
  NODE_DBG("[dcc_setup] platform_gpio_register_intr_hook - pin: %d, mask: %d\n", DccProcState.IntPin, DccProcState.IntBitmask);
  platform_gpio_register_intr_hook(DccProcState.IntBitmask, InterruptHandler);  

  gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[pin]), RISING);
  #else
  attachInterrupt( DccProcState.ExtIntNum, ExternalInterruptHandler, RISING);
  #endif

  // Set the Bits that control Multifunction or Accessory behaviour
  // and if the Accessory decoder optionally handles Output Addressing 
  // we need to peal off the top two bits
  DccProcState.cv29Value = writeCV( CV_29_CONFIG, ( readCV( CV_29_CONFIG ) & ~FLAGS_CV29_BITS ) | (Flags & FLAGS_CV29_BITS) ) ; 

  uint8_t doAutoFactoryDefault = 0;
  if((Flags & FLAGS_AUTO_FACTORY_DEFAULT) && (readCV(CV_VERSION_ID) == 255) && (readCV(CV_MANUFACTURER_ID) == 255))
  	  doAutoFactoryDefault = 1;

  writeCV( CV_VERSION_ID, VersionId ) ;
  writeCV( CV_MANUFACTURER_ID, ManufacturerId ) ;

  clearDccProcState( 0 );
  
  if(notifyCVResetFactoryDefault && doAutoFactoryDefault)
  	notifyCVResetFactoryDefault();
}

#ifndef NODEMCUDCC
////////////////////////////////////////////////////////////////////////
uint8_t NmraDcc::getCV( uint16_t CV )
{
  return readCV(CV);
}

////////////////////////////////////////////////////////////////////////
uint8_t NmraDcc::setCV( uint16_t CV, uint8_t Value)
{
  DccProcState.Flags |= FLAGS_SETCV_CALLED;
  
  uint8_t returnValue = writeCV(CV,Value);
  
  DccProcState.Flags &= ~FLAGS_SETCV_CALLED;
  
  return returnValue;
}

////////////////////////////////////////////////////////////////////////
uint16_t NmraDcc::getAddr(void)
{
  return getMyAddr();
}

////////////////////////////////////////////////////////////////////////
uint8_t NmraDcc::isSetCVReady(void)
{
  if(notifyIsSetCVReady)
	return notifyIsSetCVReady();
  return readyEEPROM();
}

////////////////////////////////////////////////////////////////////////
#ifdef DCC_DEBUG
uint8_t NmraDcc::getIntCount(void)
{
  return DccProcState.IntCount;
}

////////////////////////////////////////////////////////////////////////
uint8_t NmraDcc::getTickCount(void)
{
  return DccProcState.TickCount;
}

////////////////////////////////////////////////////////////////////////
uint8_t NmraDcc::getNestedIrqCount(void)
{
  return DccProcState.NestedIrqCount;
}

////////////////////////////////////////////////////////////////////////
uint8_t NmraDcc::getState(void)
{
  return DccRx.State;
}

////////////////////////////////////////////////////////////////////////
uint8_t NmraDcc::getBitCount(void)
{
  return DccRx.BitCount;
}
#endif

////////////////////////////////////////////////////////////////////////
void NmraDcc::setAccDecDCCAddrNextReceived(uint8_t enable)
{
  DccProcState.inAccDecDCCAddrNextReceivedMode = enable;
}
#endif //#ifndef NODEMCUDCC

////////////////////////////////////////////////////////////////////////
#ifdef NODEMCUDCC
static uint8_t process (os_param_t param, uint8_t prio)
#else
uint8_t NmraDcc::process()
#endif
{
  if( DccProcState.inServiceMode )
  {
    if( (millis() - DccProcState.LastServiceModeMillis ) > 20L )
    {
      clearDccProcState( 0 ) ;
    }
  }

  if( DccRx.DataReady )
  {
    // We need to do this check with interrupts disabled
#ifdef ESP32
    portENTER_CRITICAL(&mux);
#elif defined(NODEMCUDCC)
    ETS_GPIO_INTR_DISABLE();
#else
    noInterrupts();
#endif
    Msg = DccRx.PacketCopy ;
    DccRx.DataReady = 0 ;

#ifdef ESP32
    portEXIT_CRITICAL(&mux);
#elif defined(NODEMCUDCC)
    ETS_GPIO_INTR_ENABLE();
#else
    interrupts();
#endif
    // Checking of the XOR-byte is now done in the ISR already
    #ifdef DCC_DBGVAR
    countOf.Tel++;
    #endif
    // Clear trailing bytes
    for ( byte i=Msg.Size; i< MAX_DCC_MESSAGE_LEN; i++ ) Msg.Data[i] = 0;
    
	if( notifyDccMsg ) 	notifyDccMsg( &Msg );
		
        NODE_DBG("[dcc_process] Size: %d\tPreambleBits: %d\t%d, %d, %d, %d, %d, %d\n", 
          Msg.Size, Msg.PreambleBits, Msg.Data[0], Msg.Data[1], Msg.Data[2], Msg.Data[3], Msg.Data[4], Msg.Data[5]); 
     execDccProcessor( &Msg );
    return 1 ;
  }

  return 0 ;  
};

#ifdef NODEMCUDCC
void dcc_close()
{
  NODE_DBG("[dcc_close]\n");
  platform_gpio_mode(DccProcState.IntPin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);
}

void dcc_init()
{
  NODE_DBG("[dcc_init]\n");
  DataReady_taskid = task_get_id((task_callback_t) process);
}
#endif

