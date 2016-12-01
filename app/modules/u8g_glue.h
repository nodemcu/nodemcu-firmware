
#ifndef _U8G_GLUE_H_
#define _U8G_GLUE_H_

#include "u8g.h"

struct _lu8g_userdata_t
{
    u8g_t u8g;
    uint8_t i2c_addr;
    uint8_t use_delay;
    int cb_ref;
};
typedef struct _lu8g_userdata_t lu8g_userdata_t;

// shorthand macro for the u8g structure inside the userdata
#define LU8G (&(lud->u8g))

uint8_t u8g_com_esp8266_fbrle_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr);
uint8_t u8g_dev_gen_fb_fn(u8g_t *u8g, u8g_dev_t *dev, uint8_t msg, void *arg);

#endif
