/* ****************************************************************************
 *
 * ESP32 platform interface for WS2812 LEDs.
 *
 * Copyright (c) 2017, Arnim Laeuger
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ****************************************************************************/

#include "platform.h"
#include "platform_rmt.h"

#include "driver/rmt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/periph_defs.h"
#include "rom/gpio.h" // for gpio_matrix_out()
#include "soc/gpio_periph.h"
#include "soc/rmt_reg.h"

#undef WS2812_DEBUG

// divider to generate 100ns base period from 80MHz APB clock
#define WS2812_CLKDIV (100 * 80 /1000)

// descriptor for a ws2812 chain
typedef struct {
  bool valid;
  bool needs_reset;
  uint8_t gpio;
  rmt_item32_t reset;
  rmt_item32_t bits[2];
  const uint8_t *data;
  size_t len;
  uint8_t bitpos;
} ws2812_chain_t;

// chain descriptor array
static ws2812_chain_t ws2812_chains[RMT_CHANNEL_MAX];

static void ws2812_sample_to_rmt(const void *src, rmt_item32_t *dest, size_t src_size, size_t wanted_num, size_t *translated_size, size_t *item_num)
{
  size_t cnt_in;
  size_t cnt_out;
  const uint8_t *pucData;
  uint8_t ucData;
  uint8_t ucBitPos;
  esp_err_t tStatus;
  void *pvContext;
  ws2812_chain_t *ptContext;
  uint8_t ucBit;

  cnt_in = 0;
  cnt_out = 0;
  if( dest!=NULL && wanted_num>0 )
  {
    tStatus = rmt_translator_get_context(item_num, &pvContext);
    if( tStatus==ESP_OK )
    {
      ptContext = (ws2812_chain_t *)pvContext;

      if( ptContext->needs_reset==true )
      {
        dest[cnt_out++] = ptContext->reset;
        ptContext->needs_reset = false;
      }
      if( src!=NULL && src_size>0 )
      {
        ucBitPos = ptContext->bitpos;

        /* Each bit of the input data is converted into one RMT item. */

        pucData = (const uint8_t*)src;
        /* Get the current byte. */
        ucData = pucData[cnt_in] << ucBitPos;

        while( cnt_in<src_size && cnt_out<wanted_num )
        {
          /* Get the current bit. */
          ucBit = (ucData & 0x80U) >> 7U;
          /* Translate the bit to a WS2812 input code. */
          dest[cnt_out++] = ptContext->bits[ucBit];
          /* Move to the next bit. */
          ++ucBitPos;
          if( ucBitPos<8U )
          {
            ucData <<= 1;
          }
          else
          {
            ucBitPos = 0U;
            ++cnt_in;
            ucData = pucData[cnt_in];
          }
        }

        ptContext->bitpos = ucBitPos;
      }
    }
  }
  *translated_size = cnt_in;
  *item_num = cnt_out;
}

int platform_ws2812_setup( uint8_t gpio_num, uint32_t reset, uint32_t t0h, uint32_t t0l, uint32_t t1h, uint32_t t1l, const uint8_t *data, size_t len )
{
  int channel;

  if ((channel = platform_rmt_allocate( 1, RMT_MODE_TX )) >= 0) {
    ws2812_chain_t *chain = &(ws2812_chains[channel]);
    rmt_item32_t tRmtItem;
    uint32_t half;

    chain->valid = true;
    chain->gpio = gpio_num;
    chain->len = len;
    chain->data = data;
    chain->bitpos = 0;

    // Send a reset if "reset" is not 0.
    chain->needs_reset = (reset != 0);

    // Construct the RMT item for a reset.
    tRmtItem.level0 = 0;
    tRmtItem.level1 = 0;
    // The reset duration must fit into one RMT item. This leaves 2*15 bit,
    // which results in a maximum of 0xfffe .
    if (reset>0xfffe)
    {
      reset = 0xfffe;
    }
    if (reset>0x7fff)
    {
      tRmtItem.duration0 = 0x7fff;
      tRmtItem.duration1 = reset - 0x7fff;
    }
    else
    {
      half = reset >> 1U;
      tRmtItem.duration0 = half;
      tRmtItem.duration1 = reset - half;
    }
    chain->reset = tRmtItem;

    // Limit the bit times to the available 15 bits.
    // The values must not be 0.
    if( t0h==0 )
    {
      t0h = 1;
    }
    else if( t0h>0x7fffU )
    {
      t0h = 0x7fffU;
    }
    if( t0l==0 )
    {
      t0l = 1;
    }
    else if( t0l>0x7fffU )
    {
      t0l = 0x7fffU;
    }
    if( t1h==0 )
    {
      t1h = 1;
    }
    else if( t1h>0x7fffU )
    {
      t1h = 0x7fffU;
    }
    if( t1l==0 )
    {
      t1l = 1;
    }
    else if( t1l>0x7fffU )
    {
      t1l = 0x7fffU;
    }

    // Construct the RMT item for a 0 bit.
    tRmtItem.level0 = 1;
    tRmtItem.duration0 = t0h;
    tRmtItem.level1 = 0;
    tRmtItem.duration1 = t0l;
    chain->bits[0] = tRmtItem;

    // Construct the RMT item for a 1 bit.
    tRmtItem.level0 = 1;
    tRmtItem.duration0 = t1h;
    tRmtItem.level1 = 0;
    tRmtItem.duration1 = t1l;
    chain->bits[1] = tRmtItem;

#ifdef WS2812_DEBUG
    ESP_LOGI("ws2812", "Setup done for gpio %d on RMT channel %d", gpio_num, channel);
#endif

    return PLATFORM_OK;
  }

  return PLATFORM_ERR;
}

int platform_ws2812_release( void )
{
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX; channel++) {
    ws2812_chain_t *chain = &(ws2812_chains[channel]);

    if (chain->valid) {
      rmt_driver_uninstall( channel );

      platform_rmt_release( channel );
      chain->valid = false;

      // attach GPIO to pin, driving 0
      gpio_set_level( chain->gpio, 0 );
      gpio_set_direction( chain->gpio, GPIO_MODE_DEF_OUTPUT );
      gpio_matrix_out( chain->gpio, SIG_GPIO_OUT_IDX, 0, 0 );
    }

  }

  return PLATFORM_OK;
}

int platform_ws2812_send( void )
{
  rmt_config_t rmt_tx;
  int res = PLATFORM_OK;

  // common settings
  rmt_tx.mem_block_num = 1;
  rmt_tx.clk_div = WS2812_CLKDIV;
  rmt_tx.flags = 0;
  rmt_tx.tx_config.loop_en = false;
  rmt_tx.tx_config.carrier_en = false;
  rmt_tx.tx_config.idle_level = 0;
  rmt_tx.tx_config.idle_output_en = true;
  rmt_tx.rmt_mode = RMT_MODE_TX;

  // configure selected RMT channels
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX && res == PLATFORM_OK; channel++) {
    if (ws2812_chains[channel].valid) {
      rmt_tx.channel = channel;
      rmt_tx.gpio_num = ws2812_chains[channel].gpio;
      if (rmt_config( &rmt_tx ) != ESP_OK) {
        res = PLATFORM_ERR;
        break;
      }
      if (rmt_driver_install( channel, 0, PLATFORM_RMT_INTR_FLAGS ) != ESP_OK) {
        res = PLATFORM_ERR;
        break;
      }
      if (rmt_translator_init(channel, ws2812_sample_to_rmt) != ESP_OK) {
        res = PLATFORM_ERR;
        break;
      }
      if (rmt_translator_set_context(channel, &ws2812_chains[channel]) != ESP_OK) {
        res = PLATFORM_ERR;
        break;
      }
    }
  }

  // Try to add all channels to a group. This moves the start of all RMT sequences closer
  // together.
#if SOC_RMT_SUPPORT_TX_SYNCHRO
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX && res == PLATFORM_OK; channel++) {
    if (ws2812_chains[channel].valid) {
      if (rmt_add_channel_to_group( channel ) != ESP_OK) {
        res = PLATFORM_ERR;
        break;
      }
    }
  }
#endif

  // start selected channels one by one
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX && res == PLATFORM_OK; channel++) {
    if (ws2812_chains[channel].valid) {
      // ws2812_sample_to_rmt takes care of translating the data to rmt_item32_t
      // format, as well as prepending the reset sequence.
      if (rmt_write_sample(channel, ws2812_chains[channel].data, ws2812_chains[channel].len, false) != ESP_OK) {
        res = PLATFORM_ERR;
        break;
      }
    }
  }

  // wait for all channels to finish
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX; channel++) {
    if (ws2812_chains[channel].valid) {
      rmt_wait_tx_done( channel, portMAX_DELAY );
    }
  }

#if SOC_RMT_SUPPORT_TX_SYNCHRO
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX; channel++) {
    if (ws2812_chains[channel].valid) {
      if (rmt_remove_channel_from_group( channel ) != ESP_OK) {
        res = PLATFORM_ERR;
        break;
      }
    }
  }
#endif

  return res;
}

void platform_ws2812_init( void )
{
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX; channel++) {
    ws2812_chains[channel].valid = false;
  }
}
