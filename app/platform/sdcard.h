#ifndef _SDCARD_H
#define _SDCARD_H

#include "c_types.h"

int platform_sdcard_init( uint8_t spi_no, uint8_t ss_pin );
int platform_sdcard_status( void );
int platform_sdcard_error( void );
int platform_sdcard_type( void );
int platform_sdcard_read_block( uint8_t ss_pin, uint32_t block, uint8_t *dst );
int platform_sdcard_read_blocks( uint8_t ss_pin, uint32_t block, size_t num, uint8_t *dst );
int platform_sdcard_read_csd( uint8_t ss_pin, uint8_t *csd );
int platform_sdcard_read_cid( uint8_t ss_pin, uint8_t *cid );
int platform_sdcard_write_block( uint8_t ss_pin, uint32_t block, const uint8_t *src );
int platform_sdcard_write_blocks( uint8_t ss_pin, uint32_t block, size_t num, const uint8_t *src );

#endif
