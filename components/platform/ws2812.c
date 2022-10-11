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

#undef WS2812_DEBUG


// divider to generate 100ns base period from 80MHz APB clock
#define WS2812_CLKDIV (100 * 80 /1000)
// bit H & L durations in multiples of 100ns
#define WS2812_DURATION_T0H 4
#define WS2812_DURATION_T0L 7
#define WS2812_DURATION_T1H 8
#define WS2812_DURATION_T1L 6
#define WS2812_DURATION_RESET (50000 / 100)

// 0 bit in rmt encoding
const rmt_item32_t ws2812_rmt_bit0 = {
  .level0    = 1,
  .duration0 = WS2812_DURATION_T0H,
  .level1    = 0,
  .duration1 = WS2812_DURATION_T0L
};
// 1 bit in rmt encoding
const rmt_item32_t ws2812_rmt_bit1 = {
  .level0    = 1,
  .duration0 = WS2812_DURATION_T1H,
  .level1    = 0,
  .duration1 = WS2812_DURATION_T1L
};

#define ws2812_rmt_reset {.level0 = 0, .duration0 = 4, .level1 = 0, .duration1 = 4}
// reset signal, spans one complete buffer block
const rmt_item32_t ws2812_rmt_reset_block[64] = { [0 ... 63] = ws2812_rmt_reset };


// descriptor for a ws2812 chain
typedef struct {
  bool valid;
  uint8_t gpio;
  const uint8_t *data;
  size_t len;
  size_t tx_offset;
} ws2812_chain_t;


// chain descriptor array
static ws2812_chain_t ws2812_chains[RMT_CHANNEL_MAX];

// interrupt handler for ws2812 ISR
static intr_handle_t ws2812_intr_handle;


static void ws2812_fill_memory_encoded( rmt_channel_t channel, const uint8_t *data, size_t len, size_t tx_offset )
{
  for (size_t idx = 0; idx < len; idx++) {
    uint8_t byte = data[idx];

    for (uint8_t i = 0; i < 8; i++) {
      RMTMEM.chan[channel].data32[tx_offset + idx*8 + i].val = byte & 0x80 ? ws2812_rmt_bit1.val : ws2812_rmt_bit0.val;

      byte <<= 1;
    }
  }
}


static void ws2812_isr(void *arg)
{
  uint32_t intr_st = RMT.int_st.val;

  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX; channel++) {

    if ((intr_st & (BIT(channel+24))) && (ws2812_chains[channel].valid)) {
      RMT.int_clr.val = BIT(channel+24);

      ws2812_chain_t *chain = &(ws2812_chains[channel]);
#if defined(CONFIG_IDF_TARGET_ESP32)
      uint32_t data_sub_len = RMT.tx_lim_ch[channel].limit/8;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
      uint32_t data_sub_len = RMT.chn_tx_lim[channel].tx_lim_chn/8;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
      uint32_t data_sub_len = RMT.tx_lim_ch[channel].tx_lim/8;
#else
      uint32_t data_sub_len = RMT.tx_lim[channel].limit/8;
#endif

      if (chain->len >= data_sub_len) {
        ws2812_fill_memory_encoded( channel, chain->data, data_sub_len, chain->tx_offset );
        chain->data += data_sub_len;
        chain->len -= data_sub_len;
      } else if (chain->len == 0) {
        RMTMEM.chan[channel].data32[chain->tx_offset].val = 0;
      } else {
        ws2812_fill_memory_encoded( channel, chain->data, chain->len, chain->tx_offset );
        RMTMEM.chan[channel].data32[chain->tx_offset + chain->len*8].val = 0;
        chain->data += chain->len;
        chain->len = 0;
      }
      if (chain->tx_offset == 0) {
        chain->tx_offset = data_sub_len * 8;
      } else {
        chain->tx_offset = 0;
      }
    }
  }
}


int platform_ws2812_setup( uint8_t gpio_num, uint8_t num_mem, const uint8_t *data, size_t len )
{
  int channel;

  if ((channel = platform_rmt_allocate( num_mem, RMT_MODE_TX )) >= 0) {
    ws2812_chain_t *chain = &(ws2812_chains[channel]);

    chain->valid = true;
    chain->gpio = gpio_num;
    chain->len = len;
    chain->data = data;
    chain->tx_offset = 0;

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
    }
  }


  // hook-in our shared ISR
  esp_intr_alloc( ETS_RMT_INTR_SOURCE, PLATFORM_RMT_INTR_FLAGS, ws2812_isr, NULL, &ws2812_intr_handle );


  // start selected channels one by one
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX && res == PLATFORM_OK; channel++) {
    if (ws2812_chains[channel].valid) {
      // we just feed a single block for generating the reset to the rmt driver
      // the actual payload data is handled by our private shared ISR
      if (rmt_write_items( channel,
                           (rmt_item32_t *)ws2812_rmt_reset_block,
                           64,
                           false ) != ESP_OK) {
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


  // un-hook our ISR
  esp_intr_free( ws2812_intr_handle );


  return res;
}

void platform_ws2812_init( void )
{
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX; channel++) {
    ws2812_chains[channel].valid = false;
  }
}
