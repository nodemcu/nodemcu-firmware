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

// If either of these fails, the reset logic in ws2812_sample_to_rmt will need revisiting.
_Static_assert(SOC_RMT_MEM_WORDS_PER_CHANNEL % 8 == 0,
  "SOC_RMT_MEM_WORDS_PER_CHANNEL is assumed to be a multiple of 8");
_Static_assert(SOC_RMT_MEM_WORDS_PER_CHANNEL >= 16,
  "SOC_RMT_MEM_WORDS_PER_CHANNEL is assumed to be >= 16");

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

// This is one eighth of 512 * 100ns, ie in total a bit above the requisite 50us
const rmt_item32_t ws2812_rmt_reset = { .level0 = 0, .duration0 = 32, .level1 = 0, .duration1 = 32 };

// descriptor for a ws2812 chain
typedef struct {
  bool valid;
  bool needs_reset;
  uint8_t gpio;
  const uint8_t *data;
  size_t len;
} ws2812_chain_t;

// chain descriptor array
static ws2812_chain_t ws2812_chains[RMT_CHANNEL_MAX];

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void ws2812_sample_to_rmt(const void *src, rmt_item32_t *dest, size_t src_size, size_t wanted_num, size_t *translated_size, size_t *item_num)
{
  // Note: enabling these commented-out logs will ruin the timing so nothing
  // will actually work when they're enabled. But I've kept them in as comments
  // because they were useful in debugging the buffer management.
  // ESP_DRAM_LOGW("ws2812", "ws2812_sample_to_rmt wanted=%u src_size=%u", wanted_num, src_size);

  void *ctx;
  rmt_translator_get_context(item_num, &ctx);
  ws2812_chain_t *chain = (ws2812_chain_t *)ctx;

  size_t reset_num = 0;
  if (chain->needs_reset) {
    // Haven't sent reset yet

    // We split the reset into 8 even though it would fit in a single
    // rmt_item32_t, simply so that dest stays 8-item aligned which means we
    // don't have to worry about having to split a byte of src across multiple
    // blocks (assuming the static asserts at the top of this file are true).
    for (int i = 0; i < 8; i++) {
      dest[i] = ws2812_rmt_reset;
    }
    dest += 8;
    wanted_num -= 8;
    reset_num = 8;
    chain->needs_reset = false;
  }

  // Now write the actual data from src
  const uint8_t *data = (const uint8_t *)src;
  size_t data_num = MIN(wanted_num, src_size * 8) / 8;
  for (size_t idx = 0; idx < data_num; idx++) {
    uint8_t byte = data[idx];
    for (uint8_t i = 0; i < 8; i++) {
      dest[idx * 8 + i] = (byte & 0x80) ? ws2812_rmt_bit1 : ws2812_rmt_bit0;
      byte <<= 1;
    }
  }

  *translated_size = data_num;
  *item_num = reset_num + data_num * 8;
  // ESP_DRAM_LOGW("ws2812", "src bytes consumed: %u total rmt items: %u", *translated_size, *item_num);
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
    chain->needs_reset = true;

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

  return res;
}

void platform_ws2812_init( void )
{
  for (rmt_channel_t channel = 0; channel < RMT_CHANNEL_MAX; channel++) {
    ws2812_chains[channel].valid = false;
  }
}
