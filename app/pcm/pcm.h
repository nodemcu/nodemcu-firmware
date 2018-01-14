

#ifndef _PCM_H
#define _PCM_H


#include "task/task.h"
#include "platform.h"


//#define DEBUG_PIN 2

#ifdef DEBUG_PIN
#  define dbg_platform_gpio_write(level)     platform_gpio_write( DEBUG_PIN, level )
#  define dbg_platform_gpio_mode(mode, pull) platform_gpio_mode( DEBUG_PIN, mode, pull)
#else
#  define dbg_platform_gpio_write(level)
#  define dbg_platform_gpio_mode(mode, pull)
#endif


#define BASE_RATE 1000000

enum pcm_driver_index {
  PCM_DRIVER_SD  = 0,
  PCM_DRIVER_END = 1
};

enum pcm_rate_index {
  PCM_RATE_1K  = 0,
  PCM_RATE_2K  = 1,
  PCM_RATE_4K  = 2,
  PCM_RATE_5K  = 3,
  PCM_RATE_8K  = 4,
  PCM_RATE_10K = 5,
  PCM_RATE_12K = 6,
  PCM_RATE_16K = 7,
};

static const uint16_t pcm_rate_def[] = {BASE_RATE / 1000,  BASE_RATE / 2000, BASE_RATE / 4000,
                                        BASE_RATE / 5000,  BASE_RATE / 8000, BASE_RATE / 10000,
                                        BASE_RATE / 12000, BASE_RATE / 16000};

typedef struct {
  // available bytes in buffer
  size_t len;
  // next read position
  size_t rpos;
  // empty flag
  uint8_t empty;
  // allocated size
  size_t buf_size;
  uint8_t *data;
} pcm_buf_t;

typedef struct {
  // selected sample rate
  int rate;
  // ISR throttle control
  //   -1 = ISR and data task throttled
  //    1 = ISR throttled
  //    0 = all running
  sint8_t isr_throttled;
  // buffer selectors
  uint8_t rbuf_idx;   // read by ISR
  uint8_t fbuf_idx;   // fill by data task
  // callback fn refs
  int self_ref;
    int cb_data_ref, cb_drained_ref, cb_paused_ref, cb_stopped_ref, cb_vu_ref;
  // data buffers
  pcm_buf_t bufs[2];
  // vu measuring
  uint8_t  vu_freq;
  uint16_t vu_req_samples, vu_samples_tmp;
  uint16_t vu_peak_tmp, vu_peak;
  // sigma-delta: output pin
  int pin;
} cfg_t;

typedef uint8_t (*drv_fn_t)(cfg_t *);

typedef struct {
  drv_fn_t init;
  drv_fn_t close;
  drv_fn_t play;
  drv_fn_t record;
  drv_fn_t stop;
} drv_t;


typedef struct {
  cfg_t cfg;
  const drv_t *drv;
} pud_t;


void pcm_data_vu( task_param_t param, uint8 prio );
void pcm_data_play( task_param_t param, uint8 prio );

// task handles
extern task_handle_t pcm_data_vu_task, pcm_data_play_task, pcm_start_play_task;

#endif /* _PCM_H */
