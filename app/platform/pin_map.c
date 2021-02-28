#include "pin_map.h"
#include "eagle_soc.h"
#include "mem.h"
#include "osapi.h"

uint32_t pin_mux[GPIO_PIN_NUM];
uint8_t  pin_num[GPIO_PIN_NUM];
uint8_t  pin_func[GPIO_PIN_NUM];
#ifdef GPIO_INTERRUPT_ENABLE
uint8_t  pin_num_inv[GPIO_PIN_NUM_INV];
uint8_t  pin_int_type[GPIO_PIN_NUM];
GPIO_INT_COUNTER pin_counter[GPIO_PIN_NUM];
#endif

typedef struct {
  int8  mux;
  uint8 num;
  uint8 func;
  uint8 intr_type;
} pin_rec;
#define DECLARE_PIN(n,p) { (PERIPHS_IO_MUX_##p##_U - PERIPHS_IO_MUX), n, FUNC_GPIO##n, GPIO_PIN_INTR_DISABLE}
static const pin_rec pin_map[] = {
   {PAD_XPD_DCDC_CONF  - PERIPHS_IO_MUX, 16, 0, GPIO_PIN_INTR_DISABLE},
    DECLARE_PIN( 5, GPIO5),
    DECLARE_PIN( 4, GPIO4),
    DECLARE_PIN( 0, GPIO0),
    DECLARE_PIN( 2, GPIO2),
    DECLARE_PIN(14, MTMS),
    DECLARE_PIN(12, MTDI),
    DECLARE_PIN(13, MTCK),
    DECLARE_PIN(15, MTDO),
    DECLARE_PIN( 3, U0RXD),
    DECLARE_PIN( 1, U0TXD),
    DECLARE_PIN( 9, SD_DATA2),
    DECLARE_PIN(10, SD_DATA3)
};
void get_pin_map(void) {
  /*
   * Flash copy of the pin map.  This has to be copied to RAM to be accessible from the ISR.
   * Note that the mux field is a signed offset from PERIPHS_IO_MUX to allow the whole struct
   * to be stored in a single 32-bit record.
   */
  int i;
  /* Take temporary stack copy to avoid unaligned exceptions on Flash version */
  pin_rec pin[GPIO_PIN_NUM];
  os_memcpy(pin, pin_map, sizeof(pin_map) );

  for (i=0; i<GPIO_PIN_NUM; i++) {
    pin_mux[i]  = pin[i].mux + PERIPHS_IO_MUX;
    pin_func[i] = pin[i].func;
    pin_num[i]  = pin[i].num;
#ifdef GPIO_INTERRUPT_ENABLE
    pin_num_inv[pin_num[i]] = i;
    pin_int_type[i]         = pin[i].intr_type;
#endif
  }
}

