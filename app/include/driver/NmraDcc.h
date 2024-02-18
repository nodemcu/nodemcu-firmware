//------------------------------------------------------------------------
//
// Model Railroading with Arduino - NmraDcc.h 
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
// file:      NmraDcc.h
// author:    Alex Shepherd
// webpage:   http://mrrwa.org/
// history:   2008-03-20 Initial Version
//            2011-06-26 Migrated into Arduino library from OpenDCC codebase
//            2014 Added getAddr to NmraDcc  Geoff Bunza
//            2015-11-06 Martin Pischky (martin@pischky.de):
//                       Experimental Version to support 14 speed steps
//                       and new signature of notifyDccSpeed and notifyDccFunc
//            2017-11-29 Ken West (kgw4449@gmail.com):
//                       Added method and callback headers.
//
//------------------------------------------------------------------------
//
// purpose:   Provide a simplified interface to decode NMRA DCC packets
//			  and build DCC MutliFunction and Stationary Decoders
//
//------------------------------------------------------------------------

// NodeMCU Lua port by @voborsky

#define NODEMCUDCC
// #define NODE_DEBUG
// #define DCC_DEBUG
// #define DCC_DBGVAR

// Uncomment the following Line to Enable Service Mode CV Programming
#define NMRA_DCC_PROCESS_SERVICEMODE

// Uncomment the following line to Enable MultiFunction Decoder Operations
#define NMRA_DCC_PROCESS_MULTIFUNCTION

// Uncomment the following line to Enable 14 Speed Step Support
//#define NMRA_DCC_ENABLE_14_SPEED_STEP_MODE

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#elif defined(NODEMCUDCC)
#else
#include "WProgram.h"
#endif

#ifndef NMRADCC_IS_IN
#define NMRADCC_IS_IN

#define NMRADCC_VERSION     206     // Version 2.0.6

#define MAX_DCC_MESSAGE_LEN 6    // including XOR-Byte

//#define ALLOW_NESTED_IRQ      // uncomment to enable nested IRQ's ( only for AVR! )

typedef struct
{
	uint8_t	Size ;
	uint8_t	PreambleBits ;
	uint8_t Data[MAX_DCC_MESSAGE_LEN] ;
} DCC_MSG ;

//--------------------------------------------------------------------------
//  This section contains the NMRA Assigned DCC Manufacturer Id Codes that
//  are used in projects
//
//  This value is to be used for CV8 
//--------------------------------------------------------------------------

#define MAN_ID_JMRI             0x12
#define MAN_ID_DIY              0x0D
#define MAN_ID_SILICON_RAILWAY  0x21

//--------------------------------------------------------------------------
//  This section contains the Product/Version Id Codes for projects
//
//  This value is to be used for CV7 
//
//  NOTE: Each Product/Version Id Code needs to be UNIQUE for that particular
//  the DCC Manufacturer Id Code
//--------------------------------------------------------------------------

// Product/Version Id Codes allocated under: MAN_ID_JMRI
 
// Product/Version Id Codes allocated under: MAN_ID_DIY

// Standard CV Addresses
#define CV_ACCESSORY_DECODER_ADDRESS_LSB       1
#define CV_ACCESSORY_DECODER_ADDRESS_MSB       9

#define CV_MULTIFUNCTION_PRIMARY_ADDRESS       1
#define CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB 17
#define CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB 18

#define CV_VERSION_ID                          7
#define CV_MANUFACTURER_ID                     8
#define CV_29_CONFIG                          29

#if defined(ESP32)
	#include <esp_spi_flash.h>
	#define MAXCV     SPI_FLASH_SEC_SIZE
#elif defined(ESP8266)
	#include <spi_flash.h>
	#define MAXCV     SPI_FLASH_SEC_SIZE
#elif defined( __STM32F1__)
	#define MAXCV	(EEPROM_PAGE_SIZE/4 - 1)	// number of storage places (CV address could be larger
											// because STM32 uses virtual adresses)
    #undef ALLOW_NESTED_IRQ                 // This is done with NVIC on STM32
    #define PRIO_DCC_IRQ    9
    #define PRIO_SYSTIC     8               // MUST be higher priority than DCC Irq
#else
	#define MAXCV    E2END     					// the upper limit of the CV value currently defined to max memory.
#endif

typedef enum {
    CV29_LOCO_DIR            = 0b00000001,	/** bit 0: Locomotive Direction: "0" = normal, "1" = reversed */
    CV29_F0_LOCATION         = 0b00000010,	/** bit 1: F0 location: "0" = bit 4 in Speed and Direction instructions, "1" = bit 4 in function group one instruction */
    CV29_APS                 = 0b00000100,	/** bit 2: Alternate Power Source (APS) "0" = NMRA Digital only, "1" = Alternate power source set by CV12 */
	CV29_RAILCOM_ENABLE      = 0b00001000, 	/** bit 3: BiDi ( RailCom ) is active */
    CV29_SPEED_TABLE_ENABLE  = 0b00010000, 	/** bit 4: STE, Speed Table Enable, "0" = values in CVs 2, 4 and 6, "1" = Custom table selected by CV 25 */
    CV29_EXT_ADDRESSING      = 0b00100000,	/** bit 5: "0" = one byte addressing, "1" = two byte addressing */
    CV29_OUTPUT_ADDRESS_MODE = 0b01000000,	/** bit 6: "0" = Decoder Address Mode "1" = Output Address Mode */
    CV29_ACCESSORY_DECODER   = 0b10000000,	/** bit 7: "0" = Multi-Function Decoder Mode "1" = Accessory Decoder Mode */
} CV_29_BITS;

typedef enum {
#ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
    SPEED_STEP_14 = 15,			/**< ESTOP=0, 1 to 15 */
#endif
    SPEED_STEP_28 = 29,			/**< ESTOP=0, 1 to 29 */
    SPEED_STEP_128 = 127		/**< ESTOP=0, 1 to 127 */ 
} DCC_SPEED_STEPS;

typedef enum {
    DCC_DIR_REV = 0,     /** The locomotive to go in the reverse direction */
    DCC_DIR_FWD = 1,     /** The locomotive should move in the forward direction */
} DCC_DIRECTION;

typedef enum {
    DCC_ADDR_SHORT,      /** Short address is used. The range is 0 to 127. */
    DCC_ADDR_LONG,       /** Long Address is used. The range is 1 to 10239 */
} DCC_ADDR_TYPE;

typedef enum
{
	FN_0_4 = 1,
	FN_5_8,
	FN_9_12,
	FN_13_20,
	FN_21_28,
#ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
  FN_0				 /** function light is controlled by base line package (14 speed steps) */
#endif  
} FN_GROUP;

#define FN_BIT_00	0x10
#define FN_BIT_01	0x01
#define FN_BIT_02	0x02
#define FN_BIT_03	0x04
#define FN_BIT_04	0x08

#define FN_BIT_05	0x01
#define FN_BIT_06	0x02
#define FN_BIT_07	0x04
#define FN_BIT_08	0x08

#define FN_BIT_09	0x01
#define FN_BIT_10	0x02
#define FN_BIT_11	0x04
#define FN_BIT_12	0x08

#define FN_BIT_13	0x01
#define FN_BIT_14	0x02
#define FN_BIT_15	0x04
#define FN_BIT_16	0x08
#define FN_BIT_17	0x10
#define FN_BIT_18	0x20
#define FN_BIT_19	0x40
#define FN_BIT_20	0x80

#define FN_BIT_21	0x01
#define FN_BIT_22	0x02
#define FN_BIT_23	0x04
#define FN_BIT_24	0x08
#define FN_BIT_25	0x10
#define FN_BIT_26	0x20
#define FN_BIT_27	0x40
#define FN_BIT_28	0x80

//#define DCC_DBGVAR
#ifdef DCC_DBGVAR
typedef struct countOf_t {
    unsigned long Tel;
    unsigned long Err;
}countOf_t ;

#ifdef NODEMCUDCC
countOf_t countOf;
#else
extern struct countOf_t countOf;
#endif
#endif

#ifndef NODEMCUDCC
class NmraDcc
{
  private:
    DCC_MSG Msg ;
    
  public:
    NmraDcc();
#endif

// Flag values to be logically ORed together and passed into the init() method
#define FLAGS_MY_ADDRESS_ONLY        0x01	// Only process DCC Packets with My Address
#define FLAGS_AUTO_FACTORY_DEFAULT   0x02	// Call notifyCVResetFactoryDefault() if CV 7 & 8 == 255
#define FLAGS_SETCV_CALLED           0x10   // only used internally !!
#define FLAGS_OUTPUT_ADDRESS_MODE    0x40  // CV 29/541 bit 6
#define FLAGS_DCC_ACCESSORY_DECODER  0x80  // CV 29/541 bit 7

// Flag Bits that are cloned from CV29 relating the DCC Accessory Decoder 
#define FLAGS_CV29_BITS		(FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER)

#ifndef NODEMCUDCC
  /*+
   *  pin() is called from setup() and sets up the pin used to receive DCC packets.
   *
   *  Inputs:
   *    ExtIntNum     - Interrupt number of the pin. Use digitalPinToInterrupt(ExtIntPinNum).
   *    ExtIntPinNum  - Input pin number.
   *    EnablePullup  - Set true to enable the pins pullup resistor.
   *
   *  Returns:
   *    None.
   */
  void pin( uint8_t ExtIntNum, uint8_t ExtIntPinNum, uint8_t EnablePullup); 

  /*+
   *  pin() is called from setup() and sets up the pin used to receive DCC packets.
   *  	This relies on the internal function: digitalPinToInterrupt() to map the input pin number to the right interrupt
   *
   *  Inputs:
   *    ExtIntPinNum  - Input pin number.
   *    EnablePullup  - Set true to enable the pins pullup resistor.
   *
   *  Returns:
   *    None.
   */
#ifdef digitalPinToInterrupt
void pin( uint8_t ExtIntPinNum, uint8_t EnablePullup);
#endif

  /*+
   *  init() is called from setup() after the pin() command is called.
   *  It initializes the NmDcc object and makes it ready to process packets.
   *
   *  Inputs:
   *    ManufacturerId        - Manufacturer ID returned in CV 8.
   *                            Commonly MAN_ID_DIY.
   *    VersionId             - Version ID returned in CV 7.
   *    Flags                 - ORed flags beginning with FLAGS_...
   *                            FLAGS_MY_ADDRESS_ONLY       - Only process packets with My Address.
   *                            FLAGS_DCC_ACCESSORY_DECODER - Decoder is an accessory decoder.
   *                            FLAGS_OUTPUT_ADDRESS_MODE   - This flag applies to accessory decoders only.
   *                                                          Accessory decoders normally have 4 paired outputs
   *                                                          and a single address refers to all 4 outputs.
   *                                                          Setting FLAGS_OUTPUT_ADDRESS_MODE causes each
   *                                                          address to refer to a single output.
   *    OpsModeAddressBaseCV  - Ops Mode base address. Set it to 0?
   *
   *  Returns:
   *    None.
   */
  void init( uint8_t ManufacturerId, uint8_t VersionId, uint8_t Flags, uint8_t OpsModeAddressBaseCV );

  /*+
   *  initAccessoryDecoder() is called from setup() for accessory decoders.
   *  It calls init() with FLAGS_DCC_ACCESSORY_DECODER ORed into Flags.
   *
   *  Inputs:
   *    ManufacturerId        - Manufacturer ID returned in CV 8.
   *                            Commonly MAN_ID_DIY.
   *    VersionId             - Version ID returned in CV 7.
   *    Flags                 - ORed flags beginning with FLAGS_...
   *                            FLAGS_DCC_ACCESSORY_DECODER will be set for init() call.
   *    OpsModeAddressBaseCV  - Ops Mode base address. Set it to 0?
   *
   *  Returns:
   *    None.
   */
  void initAccessoryDecoder( uint8_t ManufacturerId, uint8_t VersionId, uint8_t Flags, uint8_t OpsModeAddressBaseCV );

  /*+
   *  process() is called from loop() to process DCC packets.
   *  It must be called very frequently to keep up with the packets.
   *
   *  Inputs:
   *    None.
   *
   *  Returns:
   *    1 - Packet succesfully parsed on this call to process().
   *    0 - Packet not ready or received packet had an error.
   */
  uint8_t process();

  /*+
   *  getCV() returns the selected CV value.
   *
   *  Inputs:
   *    CV    - CV number. It must point to a valid CV.
   *
   *  Returns:
   *    Value - CV value. Invalid CV numbers will return an undefined result
   *            since nothing will have been set in that EEPROM position.
   *            Calls notifyCVRead() if it is defined.
   */
  uint8_t getCV( uint16_t CV );

  /*+
   *  setCV() sets the value of a CV.
   *
   *  Inputs:
   *    CV    - CV number. It must point to a valid CV.
   *    Value - CV value.
   *
   *  Returns:
   *    Value - CV value set by this call.
   *            since nothing will have been set in that EEPROM position.
   *            Calls notifyCVWrite() if it is defined.
   *            Calls notifyCVChange() if the value is changed by this call.
   */
  uint8_t setCV( uint16_t CV, uint8_t Value);

  /*+
   *  setAccDecDCCAddrNextReceived() enables/disables the setting of the board address from the next received turnout command
   *
   *  Inputs:
   *    enable- boolean to enable or disable the mode
   *
   *  Returns:
   */
  void setAccDecDCCAddrNextReceived(uint8_t enable);

  /*+
   *  isSetCVReady() returns 1 if EEPROM is ready to write.
   *
   *  Inputs:
   *    CV    - CV number. It must point to a valid CV.
   *    Value - CV value.
   *
   *  Returns:
   *    ready - 1 if ready to write, 0 otherwise. AVR processor will block
   *            for several ms. for each write cycle so you should check this to avoid blocks.
   *            Note: It returns the value returned by notifyIsSetCVReady() if it is defined.
   *            Calls notifyIsSetCVReady() if it is defined.
   */
  uint8_t isSetCVReady( void );

  /*+
   *  getAddr() return the currently active decoder address.
   *            based on decoder type and current address size.
   *
   *  Inputs:
   *    None.
   *
   *  Returns:
   *    Adr - The current decoder address based on decoder type(Multifunction, Accessory)
   *          and short or long address selection for Multifunction decoders.
   */
  uint16_t getAddr(void);
	
  /*+
   *  getX()  return debugging data if DCC_DEBUG is defined.
   *          You would really need to be modifying the library to need them.
   *
   *  Inputs:
   *    None.
   *
   *  Returns:
   *    getIntCount       - Init to 0 and apparently never incremented?
   *    getTickCount      - Init to 0 and incremented each time interrupt handler
   *                        completes without an error.
   *    getBitCount       - Bit count of valid packet, 0 otherwise. Only valid until
   *                        start of the next packet.
   *    getState          - Current WAIT_... state as defined by DccRxWaitState in NmraDcc.cpp.
   *    getNestedIrqCount - Init to 0 and incremented each time the interrupt handler
   *                        is called before the previous interrupt was complete.
   *                        This is an error indication and may indicate the system
   *                        is not handling packets fast enough or some other error is occurring.
   */
// #define DCC_DEBUG
#ifdef DCC_DEBUG
  uint8_t getIntCount(void);
  uint8_t getTickCount(void);
  uint8_t getBitCount(void);
  uint8_t getState(void);
  uint8_t getNestedIrqCount(void);
#endif

};

#else
    #define DCC_RESET   1
    #define DCC_IDLE    2
    #define DCC_SPEED   3
    #define DCC_SPEED_RAW   4
    #define DCC_FUNC    5
    #define DCC_TURNOUT 6
    #define DCC_ACCESSORY   7
    #define DCC_RAW     8
    #define DCC_SERVICEMODE 9

    #define CV_VALID    10
    #define CV_READ     11
    #define CV_WRITE    12
    #define CV_RESET    13
    #define CV_ACK_COMPLETE    14


    void dcc_setup(uint8_t pin, uint8_t ManufacturerId, uint8_t VersionId, uint8_t Flags, uint8_t OpsModeAddressBaseCV);


    void dcc_close();

    void dcc_init();
#endif //#ifndef NODEMCUDCC

/************************************************************************************
    Call-back functions
************************************************************************************/

#if defined (__cplusplus)
	extern "C" {
#endif

/*+
 *  notifyDccReset(uint8_t hardReset) Callback for a DCC reset command.
 *
 *  Inputs:
 *    hardReset - 0 normal reset command.
 *                1 hard reset command.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccReset(uint8_t hardReset ) __attribute__ ((weak));

/*+
 *  notifyDccIdle() Callback for a DCC idle command.
 *
 *  Inputs:
 *    None
 *
 *  Returns:
 *    None
 */
extern void    notifyDccIdle(void) __attribute__ ((weak));


/*+
 *  notifyDccSpeed() Callback for a multifunction decoder speed command.
 *                   The received speed and direction are unpacked to separate values.
 *
 *  Inputs:
 *    Addr        - Active decoder address.
 *    AddrType    - DCC_ADDR_SHORT or DCC_ADDR_LONG.
 *    Speed       - Decoder speed. 0               = Emergency stop
 *                                 1               = Regular stop
 *                                 2 to SpeedSteps = Speed step 1 to max.
 *    Dir         - DCC_DIR_REV or DCC_DIR_FWD
 *    SpeedSteps  - Highest speed, SPEED_STEP_14   =  15
 *                                 SPEED_STEP_28   =  29
 *                                 SPEED_STEP_128  = 127
 *
 *  Returns:
 *    None
 */
extern void    notifyDccSpeed( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps ) __attribute__ ((weak));

/*+
 *  notifyDccSpeedRaw() Callback for a multifunction decoder speed command.
 *                      The value in Raw is the unpacked speed command.
 *
 *  Inputs:
 *    Addr        - Active decoder address.
 *    AddrType    - DCC_ADDR_SHORT or DCC_ADDR_LONG.
 *    Raw         - Raw decoder speed command.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccSpeedRaw( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Raw) __attribute__ ((weak));

/*+
 *  notifyDccFunc() Callback for a multifunction decoder function command.
 *
 *  Inputs:
 *    Addr        - Active decoder address.
 *    AddrType    - DCC_ADDR_SHORT or DCC_ADDR_LONG.
 *    FuncGrp     - Function group. FN_0      - 14 speed step headlight function.
 *                                                                  Mask FN_BIT_00.
 *                                  FN_0_4    - Functions  0 to  4. Mask FN_BIT_00 - FN_BIT_04
 *                                  FN_5_8    - Functions  5 to  8. Mask FN_BIT_05 - FN_BIT_08
 *                                  FN_9_12   - Functions  9 to 12. Mask FN_BIT_09 - FN_BIT_12
 *                                  FN_13_20  - Functions 13 to 20. Mask FN_BIT_13 - FN_BIT_20 
 *                                  FN_21_28  - Functions 21 to 28. Mask FN_BIT_21 - FN_BIT_28
 *    FuncState   - Function state. Bitmask where active functions have a 1 at that bit.
 *                                  You must & FuncState with the appropriate
 *                                  FN_BIT_nn value to isolate a given bit.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccFunc( uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState) __attribute__ ((weak));

/*+
 *  notifyDccAccTurnoutBoard() Board oriented callback for a turnout accessory decoder.
 *                             Most useful when CV29_OUTPUT_ADDRESS_MODE is not set.
 *                             Decoders of this type have 4 paired turnout outputs per board.
 *                             OutputPower is 1 if the power is on, and 0 otherwise.
 *
 *  Inputs:
 *    BoardAddr   - Per board address. Equivalent to CV 1 LSB & CV 9 MSB.
 *    OutputPair  - Output pair number. It has a range of 0 to 3.
 *                  Equivalent to upper 2 bits of the 3 DDD bits in the accessory packet.
 *    Direction   - Turnout direction. It has a value of 0 or 1.
 *                  It is equivalent to bit 0 of the 3 DDD bits in the accessory packet.
 *    OutputPower - Output On/Off. Equivalent to packet C bit. It has these values:
 *                  0 - Output pair is off.
 *                  1 - Output pair is on.
 *
 *  Returns:
 *    None
 */
 
extern void    notifyDccAccTurnoutBoard( uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower ) __attribute__ ((weak));
/*+
 *  notifyDccAccTurnoutOutput() Output oriented callback for a turnout accessory decoder.
 *                              Most useful when CV29_OUTPUT_ADDRESS_MODE is not set.
 *                              Decoders of this type have 4 paired turnout outputs per board.
 *                              OutputPower is 1 if the power is on, and 0 otherwise.
 *
 *  Inputs:
 *    Addr        - Per output address. There will be 4 Addr addresses
 *                  per board for a standard accessory decoder with 4 output pairs.
 *    Direction   - Turnout direction. It has a value of 0 or 1.
 *                  Equivalent to bit 0 of the 3 DDD bits in the accessory packet.
 *    OutputPower - Output On/Off. Equivalent to packet C bit. It has these values:
 *                  0 - Output is off.
 *                  1 - Output is on.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower ) __attribute__ ((weak));

/*+
 *  notifyDccAccBoardAddrSet() Board oriented callback for a turnout accessory decoder.
 *                             This notification is when a new Board Address is set to the
 *                             address of the next DCC Turnout Packet that is received
 *
 *                             This is enabled via the setAccDecDCCAddrNextReceived() method above
 *
 *  Inputs:
 *    BoardAddr   - Per board address. Equivalent to CV 1 LSB & CV 9 MSB.
 *                  per board for a standard accessory decoder with 4 output pairs.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccAccBoardAddrSet( uint16_t BoardAddr) __attribute__ ((weak));

/*+
 *  notifyDccAccOutputAddrSet() Output oriented callback for a turnout accessory decoder.
 *                              This notification is when a new Output Address is set to the
 *                              address of the next DCC Turnout Packet that is received
 *
 *                             This is enabled via the setAccDecDCCAddrNextReceived() method above
 *
 *  Inputs:
 *    Addr        - Per output address. There will be 4 Addr addresses
 *                  per board for a standard accessory decoder with 4 output pairs.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccAccOutputAddrSet( uint16_t Addr) __attribute__ ((weak));

/*+
 *  notifyDccSigOutputState() Callback for a signal aspect accessory decoder.
 *                      Defined in S-9.2.1 as the Extended Accessory Decoder Control Packet.
 *
 *  Inputs:
 *    Addr        - Decoder address.
 *    State       - 6 bit command equivalent to S-9.2.1 00XXXXXX.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccSigOutputState( uint16_t Addr, uint8_t State) __attribute__ ((weak));

/*+
 *  notifyDccMsg() Raw DCC packet callback.
 *                 Called with raw DCC packet bytes.
 *
 *  Inputs:
 *    Msg        - Pointer to DCC_MSG structure. The values are:
 *                 Msg->Size          - Number of Data bytes in the packet.
 *                 Msg->PreambleBits  - Number of preamble bits in the packet.
 *                 Msg->Data[]        - Array of data bytes in the packet.
 *
 *  Returns:
 *    None
 */
extern void    notifyDccMsg( DCC_MSG * Msg ) __attribute__ ((weak));

/*+
 *  notifyCVValid() Callback to determine if a given CV is valid.
 *                  This is called when the library needs to determine
 *                  if a CV is valid. Note: If defined, this callback
 *                  MUST determine if a CV is valid and return the
 *                  appropriate value. If this callback is not defined,
 *                  the library will determine validity.
 *
 *  Inputs:
 *    CV        - CV number.
 *    Writable  - 1 for CV writes. 0 for CV reads.
 *
 *  Returns:
 *    1         - CV is valid.
 *    0         - CV is not valid.
 */
#ifdef NODEMCUDCC
extern uint16_t notifyCVValid( uint16_t CV, uint8_t Writable ) __attribute__ ((weak));
#else
extern uint8_t notifyCVValid( uint16_t CV, uint8_t Writable ) __attribute__ ((weak));
#endif

/*+
 *  notifyCVRead()  Callback to read a CV.
 *                  This is called when the library needs to read
 *                  a CV. Note: If defined, this callback
 *                  MUST return the value of the CV.
 *                  If this callback is not defined,
 *                  the library will read the CV from EEPROM.
 *
 *  Inputs:
 *    CV        - CV number.
 *
 *  Returns:
 *    Value     - Value of the CV. Or a value > 255 to indicate error.
 */
#ifdef NODEMCUDCC
extern uint16_t notifyCVRead( uint16_t CV) __attribute__ ((weak));
#else
extern uint8_t notifyCVRead( uint16_t CV) __attribute__ ((weak));
#endif
/*+
 *  notifyCVWrite() Callback to write a value to a CV.
 *                  This is called when the library needs to write
 *                  a CV. Note: If defined, this callback
 *                  MUST write the Value to the CV and return the value of the CV.
 *                  If this callback is not defined,
 *                  the library will read the CV from EEPROM.
 *
 *  Inputs:
 *    CV        - CV number.
 *    Value     - Value of the CV.
 *
 *  Returns:
 *    Value     - Value of the CV. Or a value > 255 to signal error. 
 */
#ifdef NODEMCUDCC
extern uint16_t notifyCVWrite( uint16_t CV, uint8_t Value) __attribute__ ((weak));
#else
extern uint8_t notifyCVWrite( uint16_t CV, uint8_t Value) __attribute__ ((weak));
#endif

#ifndef NODEMCUDCC
/*+
 *  notifyIsSetCVReady()  Callback to to determine if CVs can be written.
 *                        This is called when the library needs to determine
 *                        is ready to write without blocking or failing.
 *                        Note: If defined, this callback
 *                        MUST determine if a CV write would block or fail
 *                        return the appropriate value.
 *                        If this callback is not defined,
 *                        the library determines if a write to the EEPROM
 *                        would block.
 *
 *  Inputs:
 *    None
 *
 *  Returns:
 *    1         - CV is ready to be written.
 *    0         - CV is not ready to be written.
 */
extern uint8_t notifyIsSetCVReady(void) __attribute__ ((weak));

/*+
 *  notifyCVChange()  Called when a CV value is changed.
 *                    This is called whenever a CV's value is changed.
 *  notifyDccCVChange()  Called only when a CV value is changed by a Dcc packet or a internal lib function.
 *                    it is NOT called if the CV is changed by means of the setCV() method.
 *                    Note: It is not called if notifyCVWrite() is defined
 *                    or if the value in the EEPROM is the same as the value
 *                    in the write command. 
 *
 *  Inputs:
 *    CV        - CV number.
 *    Value     - Value of the CV.
 *
 *  Returns:
 *    None
 */
extern void    notifyCVChange( uint16_t CV, uint8_t Value) __attribute__ ((weak));
extern void    notifyDccCVChange( uint16_t CV, uint8_t Value) __attribute__ ((weak));
#endif

/*+
 *  notifyCVResetFactoryDefault() Called when CVs must be reset.
 *                                This is called when CVs must be reset
 *                                to their factory defaults. This callback
 *                                should write the factory default value of
 *                                relevent CVs using the setCV() method.
 *                                setCV() must not block whens this is called.
 *                                Test with isSetCVReady() prior to calling setCV()
 *
 *  Inputs:
 *    None
 *                                                                                                        *
 *  Returns:
 *    None
 */
extern void    notifyCVResetFactoryDefault(void) __attribute__ ((weak));

/*+
 *  notifyCVAck() Called when a CV write must be acknowledged.
 *                This callback must increase the current drawn by this
 *                decoder by at least 60mA for 6ms +/- 1ms.
 *
 *  Inputs:
 *    None
 *                                                                                                        *
 *  Returns:
 *    None
 */
extern void    notifyCVAck(void) __attribute__ ((weak));
/*+
 *  notifyAdvancedCVAck() Called when a CV write must be acknowledged via Advanced Acknowledgement.
 *                This callback must send the Advanced Acknowledgement via RailComm.
 *
 *  Inputs:
 *    None
 *                                                                                                        *
 *  Returns:
 *    None
 */
extern void    notifyAdvancedCVAck(void) __attribute__ ((weak));
/*+
 *  notifyServiceMode(bool) Called when state of 'inServiceMode' changes
 *
 *  Inputs:
 *    bool  state of inServiceMode
 *                                                                                                      *
 *  Returns:
 *    None
 */
extern void    notifyServiceMode(bool) __attribute__ ((weak));

// Deprecated, only for backward compatibility with version 1.4.2. 
// Don't use in new designs. These functions may be dropped in future versions
extern void notifyDccAccState( uint16_t Addr, uint16_t BoardAddr, uint8_t OutputAddr, uint8_t State ) __attribute__ ((weak));
extern void notifyDccSigState( uint16_t Addr, uint8_t OutputIndex, uint8_t State) __attribute__ ((weak));

#if defined (__cplusplus)
}
#endif

#endif

