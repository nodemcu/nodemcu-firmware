
#include <string.h>
#include "platform_rmt.h"

#include "driver/rmt.h"


static uint8_t rmt_channel_alloc[RMT_CHANNEL_MAX];


static bool rmt_channel_check( uint8_t channel, uint8_t num_mem )
{
  if (num_mem == 0 || channel >= RMT_CHANNEL_MAX) {
    // wrong parameter
    return false;

  } else if (num_mem == 1) {
    if (rmt_channel_alloc[channel] == 0)
      return true;
    else
      return false;
  }

  return rmt_channel_check( channel-1, num_mem-1);
}

int platform_rmt_allocate( uint8_t num_mem, rmt_mode_t mode )
{
  int channel;
  int alloc_min;
  int alloc_max;
  uint8_t tag = 1;

#if SOC_RMT_TX_CANDIDATES_PER_GROUP == SOC_RMT_CHANNELS_PER_GROUP
  alloc_min = 0;
  alloc_max = RMT_CHANNEL_MAX - 1;
#else
  /* On platforms where channels cannot do both TX and RX, the TX ones always
     seem to start at index 0, and are then followed by the RX channels. */
  if( mode==RMT_MODE_TX ) {
    alloc_min = 0;
    alloc_max = SOC_RMT_TX_CANDIDATES_PER_GROUP - 1;
  } else {
    alloc_min = RMT_CHANNEL_MAX - SOC_RMT_RX_CANDIDATES_PER_GROUP;
    alloc_max = RMT_CHANNEL_MAX - 1;
  }
#endif

  for (channel = alloc_max; channel >= alloc_min; channel--) {
    if (rmt_channel_alloc[channel] == 0) {
      if (rmt_channel_check( channel, num_mem )) {
        rmt_channel_alloc[channel] = tag++;
        if (--num_mem == 0)
          break;
      }
    }
  }

  if (channel >= 0 && num_mem == 0)
    return channel;
  else
    return -1;
}


void platform_rmt_release( uint8_t channel )
{
  for ( ; channel < RMT_CHANNEL_MAX; channel++ ) {
    uint8_t tag = rmt_channel_alloc[channel];

    rmt_channel_alloc[channel] = 0;
    if (tag <= 1)
      break;
  }
}
