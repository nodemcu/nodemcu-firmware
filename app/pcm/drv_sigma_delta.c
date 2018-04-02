/*
  This file contains the sigma-delta driver implementation.
*/

#include "platform.h"
#include "hw_timer.h"
#include "task/task.h"
#include "c_stdlib.h"

#include "pcm.h"


static const os_param_t drv_sd_hw_timer_owner = 0x70636D;   // "pcm"

static void ICACHE_RAM_ATTR drv_sd_timer_isr( os_param_t arg )
{
  cfg_t *cfg = (cfg_t *)arg;
  pcm_buf_t *buf = &(cfg->bufs[cfg->rbuf_idx]);

  if (cfg->isr_throttled) {
    return;
  }

  if (!buf->empty) {
    uint16_t tmp;
    // buffer is not empty, continue reading

    tmp = abs((int16_t)(buf->data[buf->rpos]) - 128);
    if (tmp > cfg->vu_peak_tmp) {
      cfg->vu_peak_tmp = tmp;
    }
    cfg->vu_samples_tmp++;
    if (cfg->vu_samples_tmp >= cfg->vu_req_samples) {
      cfg->vu_peak = cfg->vu_peak_tmp;

      task_post_low( pcm_data_vu_task, (os_param_t)cfg );

      cfg->vu_samples_tmp = 0;
      cfg->vu_peak_tmp    = 0;
    }

    platform_sigma_delta_set_target( buf->data[buf->rpos++] );
    if (buf->rpos >= buf->len) {
      // buffer data consumed, request to re-fill it
      buf->empty = TRUE;
      cfg->fbuf_idx = cfg->rbuf_idx;
      task_post_high( pcm_data_play_task, (os_param_t)cfg );
      // switch to next buffer
      cfg->rbuf_idx ^= 1;
      dbg_platform_gpio_write( PLATFORM_GPIO_LOW );
    }
  } else {
    // flag ISR throttled
    cfg->isr_throttled = 1;
    dbg_platform_gpio_write( PLATFORM_GPIO_LOW );
    cfg->fbuf_idx = cfg->rbuf_idx;
    task_post_high( pcm_data_play_task, (os_param_t)cfg );
  }

}

static uint8_t drv_sd_stop( cfg_t *cfg )
{
  platform_hw_timer_close( drv_sd_hw_timer_owner );

  return TRUE;
}

static uint8_t drv_sd_close( cfg_t *cfg )
{
  drv_sd_stop( cfg );

  platform_sigma_delta_close( cfg->pin );

  dbg_platform_gpio_mode( PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP );

  return TRUE;
}

static uint8_t drv_sd_play( cfg_t *cfg )
{
  // VU control: derive callback frequency
  cfg->vu_req_samples = (uint16_t)((1000000L / (uint32_t)cfg->vu_freq) / (uint32_t)pcm_rate_def[cfg->rate]);
  cfg->vu_samples_tmp = 0;
  cfg->vu_peak_tmp    = 0;

  // (re)start hardware timer ISR to feed the sigma-delta
  if (platform_hw_timer_init( drv_sd_hw_timer_owner, FRC1_SOURCE, TRUE )) {
    platform_hw_timer_set_func( drv_sd_hw_timer_owner, drv_sd_timer_isr, (os_param_t)cfg );
    platform_hw_timer_arm_us( drv_sd_hw_timer_owner, pcm_rate_def[cfg->rate] );

    return TRUE;
  } else {
    return FALSE;
  }
}

static uint8_t drv_sd_init( cfg_t *cfg )
{
  dbg_platform_gpio_write( PLATFORM_GPIO_HIGH );
  dbg_platform_gpio_mode( PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP );

  platform_sigma_delta_setup( cfg->pin );
  platform_sigma_delta_set_prescale( 9 );

  return TRUE;
}

static uint8_t drv_sd_fail( cfg_t *cfg )
{
  return FALSE;
}

const drv_t pcm_drv_sd = {
  .init   = drv_sd_init,
  .close  = drv_sd_close,
  .play   = drv_sd_play,
  .record = drv_sd_fail,
  .stop   = drv_sd_stop
};
