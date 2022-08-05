#include "platform.h"
#include "driver/sigmadelta.h"
#include "driver/adc.h"
#include "driver/uart.h"
#include "soc/uart_reg.h"
#include <stdio.h>
#include <string.h>
#include <freertos/semphr.h>
#include "lua.h"
#include "rom/uart.h"
#include "esp_log.h"
#include "task/task.h"
#include "linput.h"

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include <fcntl.h>
#include <errno.h>
#endif

int platform_init (void)
{
  platform_ws2812_init();
  return PLATFORM_OK;
}


// *****************************************************************************
// GPIO subsection

int platform_gpio_exists( unsigned gpio ) { return GPIO_IS_VALID_GPIO(gpio); }
int platform_gpio_output_exists( unsigned gpio ) { return GPIO_IS_VALID_OUTPUT_GPIO(gpio); }


// ****************************************************************************
// UART

#define PLATFORM_UART_EVENT_DATA     (UART_EVENT_MAX + 1)
#define PLATFORM_UART_EVENT_OOM      (UART_EVENT_MAX + 2)
#define PLATFORM_UART_EVENT_RX       (UART_EVENT_MAX + 3)
#define PLATFORM_UART_EVENT_BREAK    (UART_EVENT_MAX + 4)

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#define ADJUST_IDR(id,retval)   if (id-- == 0) return retval
#define ADJUST_ID(id)   if (id-- == 0) return
#define UNADJUST_ID(id)   id++
#else
#define ADJUST_IDR(id, retval)
#define ADJUST_ID(id)
#define UNADJUST_ID(id)
#endif

typedef struct {
  unsigned rx_buf_sz;
  unsigned tx_buf_sz;
} uart_buf_sz_cfg_t;

static const uart_buf_sz_cfg_t uart_buf_sz_cfg[] = {
{ .rx_buf_sz = CONFIG_NODEMCU_UART_DRIVER_BUF_SIZE_RX0 +0,
  .tx_buf_sz = CONFIG_NODEMCU_UART_DRIVER_BUF_SIZE_TX0 +0 },
#if NUM_UART > 1
{ .rx_buf_sz = CONFIG_NODEMCU_UART_DRIVER_BUF_SIZE_RX1 +0,
  .tx_buf_sz = CONFIG_NODEMCU_UART_DRIVER_BUF_SIZE_TX1 +0 },
#endif
#if NUM_UART > 2
{ .rx_buf_sz = CONFIG_NODEMCU_UART_DRIVER_BUF_SIZE_RX2 +0,
  .tx_buf_sz = CONFIG_NODEMCU_UART_DRIVER_BUF_SIZE_TX2 +0 },
#endif
};

typedef struct {
  unsigned id;
  int type;
  size_t size;
  char* data;
} uart_event_post_t;

static const char *UART_TAG = "uart";

uart_status_t uart_status[NUM_UART];
task_handle_t uart_event_task_id = 0;
SemaphoreHandle_t sem = NULL;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
task_handle_t usbcdc_event_task_id = 0;
static uart_status_t usbcdc_status;
SemaphoreHandle_t usbcdc_sem = NULL;
#endif

extern bool uart_on_data_cb(unsigned id, const char *buf, size_t len);
extern bool uart_on_error_cb(unsigned id, const char *buf, size_t len);

static void handle_uart_data(unsigned int id, uart_event_post_t *post, uart_status_t *us) {
  size_t i = 0;
  while (i < post->size) {
    if (id == CONFIG_ESP_CONSOLE_UART_NUM && run_input) {
      unsigned used = feed_lua_input(post->data + i, post->size - i);
      i += used;
    } else {
      char ch = post->data[i];
      us->line_buffer[us->line_position] = ch;
      us->line_position++;

      uint16_t need_len = us->need_len;
      int16_t end_char = us->end_char;
      size_t max_wanted =
        (end_char >= 0 && need_len == 0) ? LUA_MAXINPUT : need_len;
      bool at_end = (us->line_position >= max_wanted);
      bool end_char_found =
        (end_char >= 0 && (uint8_t)ch == (uint8_t)end_char);
      if (at_end || end_char_found) {
        uart_on_data_cb(id, us->line_buffer, us->line_position);
        us->line_position = 0;
      }
      ++i;
    }
  }
}

void uart_event_task(task_param_t param, task_prio_t prio)
{
  uart_event_post_t *post = (uart_event_post_t *)param;
  unsigned id = post->id;
  uart_status_t *us = &uart_status[id];
  UNADJUST_ID(id);
  xSemaphoreGive(sem);
  if (post->type == PLATFORM_UART_EVENT_DATA)
  {
    handle_uart_data(id, post, us);
    free(post->data);
  }
  else
  {
    const char *err;
    switch (post->type)
    {
    case PLATFORM_UART_EVENT_OOM:
      err = "out_of_memory";
      break;
    case PLATFORM_UART_EVENT_BREAK:
      err = "break";
      break;
    case PLATFORM_UART_EVENT_RX:
    default:
      err = "rx_error";
    }
    uart_on_error_cb(id, err, strlen(err));
  }
  free(post);
}

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
void usbcdc_event_task(task_param_t param, task_prio_t prio)
{
  uart_event_post_t *post = (uart_event_post_t *)param;
  xSemaphoreGive(usbcdc_sem);
  handle_uart_data(0, post, &usbcdc_status);
  free(post);
}

static void task_usbcdc(void *pvParameters) {
  // 4 chosen as a number smaller than the number of nodemcu task slots
  // available, to make it unlikely we encounter a task_post failing
  if (usbcdc_sem == NULL)
    usbcdc_sem = xSemaphoreCreateCounting(4, 4);

  uart_event_post_t *post = NULL;

  for (;;) {
    if (post == NULL) {
      post = (uart_event_post_t *)malloc(sizeof(uart_event_post_t) + 64 * sizeof(char));
      if (post == NULL) {
        ESP_LOGE(UART_TAG, "Can not alloc memory in task_usbcdc()");
        // reboot here?
        continue;
      }
      post->data = (void *)(post + 1);
      post->type = PLATFORM_UART_EVENT_DATA;
    }

    int len = usb_serial_jtag_read_bytes(post->data, 64, portMAX_DELAY);

    if (len > 0) {
      for (int i = 0; i < len; i++) {
        if (post->data[i] == '\r') {
          post->data[i] = '\n';
        }
      }
      post->size = len;
      xSemaphoreTake(usbcdc_sem, portMAX_DELAY);
      if (!task_post_medium(usbcdc_event_task_id, (task_param_t)post))
      {
        ESP_LOGE(UART_TAG, "Task event overrun in task_usbcdc()");
        xSemaphoreGive(usbcdc_sem);
      } else {
        post = NULL;
      }
    }
  }
}
#endif

static void task_uart( void *pvParameters ){
  unsigned id = (unsigned)pvParameters;
  
  // 4 chosen as a number smaller than the number of nodemcu task slots
  // available, to make it unlikely we encounter a task_post failing
  if (sem == NULL)
    sem = xSemaphoreCreateCounting(4, 4);

  uart_event_post_t* post = NULL;
  uart_event_t event;

  for(;;) {
    if(xQueueReceive(uart_status[id].queue, (void * )&event, (portTickType)portMAX_DELAY)) {
      switch(event.type) {
        case UART_DATA: {
          // Attempt to coalesce received bytes to reduce risk of overrunning
          // the task event queue.
          size_t len;
          if (uart_get_buffered_data_len(id, &len) != ESP_OK)
            len = event.size;
          if (len == 0)
            continue; // we already gobbled all the bytes

          post = (uart_event_post_t*)malloc(sizeof(uart_event_post_t));
          if(post == NULL) {
            ESP_LOGE(UART_TAG, "Can not alloc memory in task_uart()");
            // reboot here?
            continue;
          }
          post->data = malloc(len);
          if(post->data == NULL) {
            ESP_LOGE(UART_TAG, "Can not alloc memory in task_uart()");
            post->id = id;
            post->type = PLATFORM_UART_EVENT_OOM;
          } else {
            post->id = id;
            post->type = PLATFORM_UART_EVENT_DATA;
            post->size = uart_read_bytes(id, (uint8_t *)post->data, len, 0);
          }
          break;
        }
        case UART_BREAK:
          post = (uart_event_post_t*)malloc(sizeof(uart_event_post_t));
          if(post == NULL) {
            ESP_LOGE(UART_TAG, "Can not alloc memory in task_uart()");
            // reboot here?
            continue;
          }
          post->id = id;
          post->type = PLATFORM_UART_EVENT_BREAK;
          post->data = NULL;
          break;
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
        case UART_PARITY_ERR:
        case UART_FRAME_ERR:
          post = (uart_event_post_t*)malloc(sizeof(uart_event_post_t));
          if(post == NULL) {
            ESP_LOGE(UART_TAG, "Can not alloc memory in task_uart()");
            // reboot here?
            continue;
          }
          post->id = id;
          post->type = PLATFORM_UART_EVENT_RX;
          post->data = NULL;
          break;
        case UART_PATTERN_DET:
        default:
          ;
      }
      if (post != NULL) {
        xSemaphoreTake(sem, portMAX_DELAY);
        if (!task_post_medium(uart_event_task_id, (task_param_t)post))
        {
          ESP_LOGE(UART_TAG, "Task event overrun in task_uart()");
          xSemaphoreGive(sem);
          free(post->data);
          free(post);
        }
        post = NULL;
      }
    }
  }
}

// pins must not be null for non-console uart
uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits, uart_pins_t* pins )
{
  ADJUST_IDR(id, baud);
  int flow_control = UART_HW_FLOWCTRL_DISABLE;
  if(pins->flow_control & PLATFORM_UART_FLOW_CTS) flow_control |= UART_HW_FLOWCTRL_CTS;
  if(pins->flow_control & PLATFORM_UART_FLOW_RTS) flow_control |= UART_HW_FLOWCTRL_RTS;
  
  uart_config_t cfg = {
     .baud_rate = baud,
     .flow_ctrl = flow_control,
     .rx_flow_ctrl_thresh = UART_FIFO_LEN - 16,
  };
  
  switch (databits)
  {
    case 5: cfg.data_bits = UART_DATA_5_BITS; break;
    case 6: cfg.data_bits = UART_DATA_6_BITS; break;
    case 7: cfg.data_bits = UART_DATA_7_BITS; break;
    case 8: // fall-through
    default: cfg.data_bits = UART_DATA_8_BITS; break;
  }
  switch (parity)
  {
    case PLATFORM_UART_PARITY_EVEN: cfg.parity = UART_PARITY_EVEN; break;
    case PLATFORM_UART_PARITY_ODD:  cfg.parity = UART_PARITY_ODD; break;
    default: // fall-through
    case PLATFORM_UART_PARITY_NONE: cfg.parity = UART_PARITY_DISABLE; break;
  }
  switch (stopbits)
  {
    default: // fall-through
    case PLATFORM_UART_STOPBITS_1:
      cfg.stop_bits = UART_STOP_BITS_1; break;
    case PLATFORM_UART_STOPBITS_1_5:
      cfg.stop_bits = UART_STOP_BITS_1_5; break;
    case PLATFORM_UART_STOPBITS_2:
      cfg.stop_bits = UART_STOP_BITS_2; break;
  }
  uart_param_config(id, &cfg);

  if (pins != NULL) {
    uart_set_pin(id, pins->tx_pin, pins->rx_pin, pins->rts_pin, pins->cts_pin);
    uart_set_line_inverse(id, (pins->tx_inverse? UART_TXD_INV_M : 0)
                                | (pins->rx_inverse? UART_RXD_INV_M : 0)
                                | (pins->rts_inverse? UART_RTS_INV_M : 0)
                                | (pins->cts_inverse? UART_CTS_INV_M : 0)
                        );
  }

  return baud;
}

void platform_uart_setmode(unsigned id, unsigned mode)
{
  uart_mode_t uartMode;

  ADJUST_IDR(id,);
  
  switch(mode)
  {
    case PLATFORM_UART_MODE_IRDA:
      uartMode = UART_MODE_IRDA; break;
    case PLATFORM_UART_MODE_RS485_COLLISION_DETECT:
      uartMode = UART_MODE_RS485_COLLISION_DETECT; break;
    case PLATFORM_UART_MODE_RS485_APP_CONTROL:
      uartMode = UART_MODE_RS485_APP_CTRL; break;
    case PLATFORM_UART_MODE_HALF_DUPLEX:
      uartMode = UART_MODE_RS485_HALF_DUPLEX; break;
    case PLATFORM_UART_MODE_UART:
    default:
      uartMode = UART_MODE_UART; break;
  }
  uart_set_mode(id, uartMode);
}

void platform_uart_send_multi( unsigned id, const char *data, size_t len )
{
  size_t i;
  if (id == CONFIG_ESP_CONSOLE_UART_NUM) {
      for( i = 0; i < len; i ++ ) {
        putchar (data[ i ]);
    }
  } else {
    ADJUST_ID(id);
    uart_write_bytes(id, data, len);
  }
}

void platform_uart_send( unsigned id, uint8_t data )
{
  if (id == CONFIG_ESP_CONSOLE_UART_NUM)
    putchar (data);
  else {
    ADJUST_ID(id);
    uart_write_bytes(id, (const char *)&data, 1);
  }
}

void platform_uart_flush( unsigned id )
{
  if (id == CONFIG_ESP_CONSOLE_UART_NUM)
    fflush (stdout);
  else {
    ADJUST_ID(id);
    uart_tx_flush(id);
  }
}


int platform_uart_start( unsigned id )
{
  if(uart_event_task_id == 0)
    uart_event_task_id = task_get_id( uart_event_task );

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  if (id == 0) {
    if (usbcdc_event_task_id == 0) {
      usbcdc_event_task_id = task_get_id(usbcdc_event_task);


      /* Disable buffering on stdin */
      setvbuf(stdin, NULL, _IONBF, 0);

      /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
      esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
      /* Move the caret to the beginning of the next line on '\n' */
      esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

      /* Enable blocking mode on stdin and stdout */
      fcntl(fileno(stdout), F_SETFL, 0);
      fcntl(fileno(stdin), F_SETFL, 0);

      usb_serial_jtag_driver_config_t usb_serial_jtag_config;
      usb_serial_jtag_config.rx_buffer_size = 1024;
      usb_serial_jtag_config.tx_buffer_size = 1024;

      esp_err_t ret = ESP_OK;
      /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes */
      ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
      if (ret != ESP_OK)
      {
        return -1;
      }

      /* Tell vfs to use usb-serial-jtag driver */
      esp_vfs_usb_serial_jtag_use_driver();
    }
    usbcdc_status.line_buffer = malloc(LUA_MAXINPUT);
    usbcdc_status.line_position = 0;
    if(usbcdc_status.line_buffer == NULL) {
      return -1;
    }

    const char *pcName = "usbcdc";
    if(xTaskCreate(task_usbcdc, pcName, 4096, (void*)id, ESP_TASK_MAIN_PRIO + 1, &usbcdc_status.taskHandle) != pdPASS) {
      free(usbcdc_status.line_buffer);
      usbcdc_status.line_buffer = NULL;
      return -1;
    }
    return 0;
  }
#endif

  ADJUST_IDR(id, -1);

  uart_status_t *us = & uart_status[id];

  esp_err_t ret = uart_driver_install(id, uart_buf_sz_cfg[id].rx_buf_sz, uart_buf_sz_cfg[id].tx_buf_sz, 3, & us->queue, 0);
  if(ret != ESP_OK) {
    return -1;
  }
  us->line_buffer = malloc(LUA_MAXINPUT);
  us->line_position = 0;
  if(us->line_buffer == NULL) {
    uart_driver_delete(id);
    return -1;
  }

  char pcName[6];
  snprintf( pcName, 6, "uart%d", id );
  pcName[5] = '\0';
  if(xTaskCreate(task_uart, pcName, 2048, (void*)id, ESP_TASK_MAIN_PRIO + 1, & us->taskHandle) != pdPASS) {
    uart_driver_delete(id);
    free(us->line_buffer);
    us->line_buffer = NULL;
    return -1;
  }

  return 0;
}

void platform_uart_stop( unsigned id )
{
  if (id == CONFIG_ESP_CONSOLE_UART_NUM)
    ;
  else {
    ADJUST_IDR(id,);
    uart_status_t *us = & uart_status[id];  
    uart_driver_delete(id);
    if(us->line_buffer) free(us->line_buffer);
    us->line_buffer = NULL;
    if(us->taskHandle) vTaskDelete(us->taskHandle);
    us->taskHandle = NULL;
  }
}

int platform_uart_get_config(unsigned id, uint32_t *baudp, uint32_t *databitsp, uint32_t *parityp, uint32_t *stopbitsp) {
    int err;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    if (id == 0) {
      // Return dummy values
      *baudp = 115200;
      *databitsp = 8;
      *parityp = PLATFORM_UART_PARITY_NONE;
      *stopbitsp = PLATFORM_UART_STOPBITS_1;
      return 0;
    }
#endif

    ADJUST_IDR(id, -1);

    err = uart_get_baudrate(id, baudp);
    if (err != ESP_OK) return -1;
    *baudp &= 0xFFFFFFFE; // round down

    uart_word_length_t databits;
    err = uart_get_word_length(id, &databits);
    if (err != ESP_OK) return -1;

    switch (databits) {
        case UART_DATA_5_BITS:
            *databitsp = 5;
            break;
        case UART_DATA_6_BITS:
            *databitsp = 6;
            break;
        case UART_DATA_7_BITS:
            *databitsp = 7;
            break;
        case UART_DATA_8_BITS:
            *databitsp = 8;
            break;
        default:
            return -1;
    }

    uart_parity_t parity;
    err = uart_get_parity(id, &parity);
    if (err != ESP_OK) return -1;
    switch(parity) {
      case UART_PARITY_DISABLE: *parityp = PLATFORM_UART_PARITY_NONE; break;
      case UART_PARITY_EVEN:    *parityp = PLATFORM_UART_PARITY_EVEN; break;
      case UART_PARITY_ODD:     *parityp = PLATFORM_UART_PARITY_ODD; break;
    }

    uart_stop_bits_t stopbits;
    err = uart_get_stop_bits(id, &stopbits);
    if (err != ESP_OK) return -1;
    switch(stopbits) {
      case UART_STOP_BITS_1:   *stopbitsp = PLATFORM_UART_STOPBITS_1; break;
      case UART_STOP_BITS_1_5: *stopbitsp = PLATFORM_UART_STOPBITS_1_5; break;
      case UART_STOP_BITS_2:   *stopbitsp = PLATFORM_UART_STOPBITS_2; break;
      case UART_STOP_BITS_MAX: break;
    }

    return 0;
}

int platform_uart_set_wakeup_threshold(unsigned id, unsigned threshold)
{
  ADJUST_IDR(id, 0);
  esp_err_t err = uart_set_wakeup_threshold(id, threshold);
  return (err == ESP_OK) ? 0 : -1;
}

// *****************************************************************************
// Sigma-Delta platform interface

static gpio_num_t platform_sigma_delta_channel2gpio[SIGMADELTA_CHANNEL_MAX];

int platform_sigma_delta_exists( unsigned channel ) {
  return (channel < SIGMADELTA_CHANNEL_MAX);
}

uint8_t platform_sigma_delta_setup( uint8_t channel, uint8_t gpio_num )
{
#if 0
  // signal generator can't be stopped this way
  // stop signal generator
  if (ESP_OK != sigmadelta_set_prescale( channel, 0 ))
    return 0;
#endif

  // note channel to gpio assignment
  platform_sigma_delta_channel2gpio[channel] = gpio_num;

  return ESP_OK == sigmadelta_set_pin( channel, gpio_num ) ? 1 : 0;
}

uint8_t platform_sigma_delta_close( uint8_t channel )
{
#if 0
  // Note: signal generator can't be stopped this way
  // stop signal generator
  if (ESP_OK != sigmadelta_set_prescale( channel, 0 ))
    return 0;
#endif

  gpio_set_level( platform_sigma_delta_channel2gpio[channel], 1 );
  gpio_config_t cfg;
  // force pin back to GPIO
  cfg.intr_type = GPIO_INTR_DISABLE;
  cfg.mode = GPIO_MODE_OUTPUT;  // essential to switch IO matrix to GPIO
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pin_bit_mask = 1 << platform_sigma_delta_channel2gpio[channel];
  if (ESP_OK != gpio_config( &cfg ))
    return 0;

  // and set it finally to input with pull-up enabled
  cfg.mode = GPIO_MODE_INPUT;

  return ESP_OK == gpio_config( &cfg ) ? 1 : 0;
}

#if 0
// PWM emulation not possible, code kept for future reference
uint8_t platform_sigma_delta_set_pwmduty( uint8_t channel, uint8_t duty )
{
  uint8_t target = 0, prescale = 0;

  target = duty > 128 ? 256 - duty : duty;
  prescale = target == 0 ? 0 : target-1;

  //freq = 80000 (khz) /256 /duty_target * (prescale+1)
  if (ESP_OK != sigmadelta_set_prescale( channel, prescale ))
    return 0;
  if (ESP_OK != sigmadelta_set_duty( channel, duty-128 ))
    return 0;

  return 1;
}
#endif

uint8_t platform_sigma_delta_set_prescale( uint8_t channel, uint8_t prescale )
{
  return ESP_OK == sigmadelta_set_prescale( channel, prescale ) ? 1 : 0;
}

uint8_t IRAM_ATTR platform_sigma_delta_set_duty( uint8_t channel, int8_t duty )
{
  return ESP_OK == sigmadelta_set_duty( channel, duty ) ? 1 : 0;
}
// *****************************************************************************
// ADC

int platform_adc_exists( uint8_t adc ) { return adc < 2 && adc > 0; }

int platform_adc_channel_exists( uint8_t adc, uint8_t channel ) {
  return (adc == 1 && channel < 8);
}

uint8_t platform_adc_set_width( uint8_t adc, int bits ) {
  (void)adc;
  bits = bits - 9;
  if (ESP_OK != adc1_config_width( bits ))
    return 0;

  return 1;
}

uint8_t platform_adc_setup( uint8_t adc, uint8_t channel, uint8_t atten ) {
  if (adc == 1 && ESP_OK != adc1_config_channel_atten( channel, atten ))
    return 0;

  return 1;
}

int platform_adc_read( uint8_t adc, uint8_t channel ) {
  int value = -1;
  if (adc == 1) value = adc1_get_raw( channel );
  return value;
}

int platform_adc_read_hall_sensor( ) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  int value = hall_sensor_read( );
  return value;
#else
  return -1;
#endif
}
// *****************************************************************************
// I2C platform interface

#if 0
// platform functions for the IDF I2C driver
// they're currently deactivated because of https://github.com/espressif/esp-idf/issues/241
// long-term goal is to use these instead of the SW driver in the #else branch

#include "driver/i2c.h"
int platform_i2c_setup( unsigned id, uint8_t sda, uint8_t scl, uint32_t speed ) {
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = sda;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_io_num = scl;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = speed;
  if (ESP_OK != i2c_param_config( id, &conf ))
    return 0;

  if (ESP_OK != i2c_driver_install( id, conf.mode, 0, 0, 0 ))
    return 0;

  return 1;
}

int platform_i2c_send_start( unsigned id ) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start( cmd );
  esp_err_t ret = i2c_master_cmd_begin( id, cmd, 1000 / portTICK_RATE_MS );
  i2c_cmd_link_delete( cmd );

  return ret == ESP_OK ? 1 : 0;
}

int platform_i2c_send_stop( unsigned id ) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_stop( cmd );
  esp_err_t ret = i2c_master_cmd_begin( id, cmd, 1000 / portTICK_RATE_MS );
  i2c_cmd_link_delete( cmd );

  return ret == ESP_OK ? 1 : 0;
}

int platform_i2c_send_address( unsigned id, uint16_t address, int direction, int ack_check_en ) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  direction = ( direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ) ? 0 : 1;

  i2c_master_write_byte( cmd, (uint8_t) ((address << 1) | direction ), ack_check_en );

  esp_err_t ret = i2c_master_cmd_begin( id, cmd, 1000 / portTICK_RATE_MS );
  i2c_cmd_link_delete( cmd );

  // we return ack (1=acked).
  if (ret == ESP_FAIL)
    return 0;
  else if (ret == ESP_OK)
    return 1;
  else
    return -1;
}

int platform_i2c_send_byte( unsigned id, uint8_t data, int ack_check_en ) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_write_byte( cmd, data, ack_check_en );

  esp_err_t ret = i2c_master_cmd_begin( id, cmd, 1000 / portTICK_RATE_MS );
  i2c_cmd_link_delete( cmd );

  // we return ack (1=acked).
  if (ret == ESP_FAIL)
    return 0;
  else if (ret == ESP_OK)
    return 1;
  else
    return -1;
}

int platform_i2c_recv_byte( unsigned id, int ack_val ){
  uint8_t data;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_read_byte( cmd, &data, ack_val > 0 ? 0 : 1 );

  esp_err_t ret = i2c_master_cmd_begin( id, cmd, 1000 / portTICK_RATE_MS );
  i2c_cmd_link_delete( cmd );

  return ret == ESP_OK ? data : -1;
}

#else

// platform functions for SW-based I2C driver
// they work around the issue with the IDF driver
// remove when functions for the IDF driver can be used instead

#include "driver/i2c_sw_master.h"
int platform_i2c_setup( unsigned id, uint8_t sda, uint8_t scl, uint32_t speed ){
  if (!platform_gpio_output_exists(sda) || !platform_gpio_output_exists(scl))
    return 0;

  if (speed != PLATFORM_I2C_SPEED_SLOW)
    return 0;

  i2c_sw_master_gpio_init(sda, scl);
  return 1;
}

int platform_i2c_send_start( unsigned id ){
  i2c_sw_master_start();
  return 1;
}

int platform_i2c_send_stop( unsigned id ){
  i2c_sw_master_stop();
  return 1;
}

int platform_i2c_send_address( unsigned id, uint16_t address, int direction, int ack_check_en ){
  // Convert enum codes to R/w bit value.
  // If TX == 0 and RX == 1, this test will be removed by the compiler
  if ( ! ( PLATFORM_I2C_DIRECTION_TRANSMITTER == 0 &&
           PLATFORM_I2C_DIRECTION_RECEIVER == 1 ) ) {
    direction = ( direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ) ? 0 : 1;
  }

  i2c_sw_master_writeByte( (uint8_t) ((address << 1) | direction ));
  // Low-level returns nack (0=acked); we return ack (1=acked).
  return ! i2c_sw_master_getAck();
}

int platform_i2c_send_byte( unsigned id, uint8_t data, int ack_check_en ){
  i2c_sw_master_writeByte(data);
  // Low-level returns nack (0=acked); we return ack (1=acked).
  return ! i2c_sw_master_getAck();
}

int platform_i2c_recv_byte( unsigned id, int ack ){
  uint8_t r = i2c_sw_master_readByte();
  i2c_sw_master_setAck( !ack );
  return r;
}
#endif

int platform_i2c_exists( unsigned id ) { return id < I2C_NUM_MAX; }


void platform_print_deprecation_note( const char *msg, const char *time_frame)
{
  printf( "Warning, deprecated API! %s. It will be removed %s. See documentation for details.\n", msg, time_frame );
}

