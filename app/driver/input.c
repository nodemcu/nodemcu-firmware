#include "platform.h"
#include "driver/uart.h"
#include "driver/input.h"
#include <stdint.h>
#include "mem.h"

static void input_handler(platform_task_param_t flag, uint8 priority);

static struct input_state {
  char       *data;
  int         line_pos;
  size_t      len;
  const char *prompt;
  uart_cb_t   uart_cb;
  platform_task_handle_t input_sig;
  int         data_len;
  bool        run_input;
  bool        uart_echo;
  char        last_char;
  char        end_char;
  uint8       input_sig_flag;
} ins = {0};

#define NUL '\0'
#define BS  '\010'
#define CR  '\r'
#define LF  '\n'
#define DEL  0x7f
#define BS_OVER "\010 \010"

#define sendStr(s)  uart0_sendStr(s)
#define putc(c)     uart0_putc(c)

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;

static bool uart_getc(char *c){
    RcvMsgBuff *pRxBuff = &(UartDev.rcv_buff);
    if(pRxBuff->pWritePos == pRxBuff->pReadPos){   // empty
        return false;
    }
    // ETS_UART_INTR_DISABLE();
    ETS_INTR_LOCK();
    *c = (char)*(pRxBuff->pReadPos);
    if (pRxBuff->pReadPos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE)) {
        pRxBuff->pReadPos = pRxBuff->pRcvMsgBuff ;
    } else {
        pRxBuff->pReadPos++;
    }
    // ETS_UART_INTR_ENABLE();
    ETS_INTR_UNLOCK();
    return true;
}

/*
** input_handler at high-priority is a system post task used to process pending Rx
** data on UART0.  The flag is used as a latch to stop the interrupt handler posting
** multiple pending requests.  At low priority it is used the trigger interactive
** compile.
**
** The ins.data check detects up the first task call which used to initialise
** everything.
*/
extern int lua_main (void);
static bool input_readline(void);

static void input_handler(platform_task_param_t flag, uint8 priority) {
  (void) priority;
  if (!ins.data) {
    lua_main();
    return;
  }
  ins.input_sig_flag = flag & 0x1;
  while (input_readline()) {}
}

/*
** The input state (ins) is private, so input_setup() exposes the necessary
** access to public properties and is called in user_init() before the Lua
** enviroment is initialised.  The second routine input_setup_receive() is
** called in lua.c after the Lua environment is available to bind the Lua
** input handler.  Any UART input before this receive setup is ignored.
*/
void input_setup(int bufsize, const char *prompt) {
  // Initialise non-zero elements
  ins.run_input = true;
  ins.uart_echo = true;
  ins.data      = os_malloc(bufsize);
  ins.len       = bufsize;
  ins.prompt    = prompt;
  ins.input_sig = platform_task_get_id(input_handler);
  // pass the task CB parameters to the uart driver
  uart_init_task(ins.input_sig, &ins.input_sig_flag);
  ETS_UART_INTR_ENABLE();
}

void input_setup_receive(uart_cb_t uart_on_data_cb, int data_len, char end_char, bool run_input) {
  ins.uart_cb   = uart_on_data_cb;
  ins.data_len  = data_len;
  ins.end_char  = end_char;
  ins.run_input = run_input;
}

void input_setecho (bool flag) {
  ins.uart_echo = flag;
}

void input_setprompt (const char *prompt) {
  ins.prompt = prompt;
}

/*
** input_readline() is called from the input_handler() event routine which is
** posted by the UART Rx ISR posts. This works in one of two modes depending on
** the bool ins.run_input.
** -  TRUE:   it clears the UART FIFO up to EOL, doing any callback and sending
**            the line to Lua.
** -  FALSE:  it clears the UART FIFO doing callbacks according to the data_len
**            or end_char break.
*/
extern void lua_input_string (const char *line, int len);

static bool input_readline(void) {
  char ch = NUL;
  if (ins.run_input) {
    while (uart_getc(&ch)) {
      /* handle CR & LF characters and aggregate \n\r and \r\n pairs */
      if ((ch == CR && ins.last_char == LF) ||
          (ch == LF && ins.last_char == CR)) {
        ins.last_char = NUL;
        continue;
      }

      /* backspace key */
      if (ch == DEL || ch == BS) {
        if (ins.line_pos > 0) {
          if(ins.uart_echo) sendStr(BS_OVER);
          ins.line_pos--;
        }
        ins.data[ins.line_pos] = 0;
        ins.last_char = NUL;
        continue;
      }
      ins.last_char = ch;

      /* end of data */
      if (ch == CR || ch == LF) {
        if (ins.uart_echo) putc(LF);
        if (ins.uart_cb) ins.uart_cb(ins.data, ins.line_pos);
        if (ins.line_pos == 0) {
          /* Get a empty data, then go to get a new data */

          sendStr(ins.prompt);
          continue;
        } else {
            ins.data[ins.line_pos++] = LF;
            lua_input_string(ins.data, ins.line_pos);
            ins.line_pos = 0;
          return true;
        }
      }

      if(ins.uart_echo) putc(ch);

      /* it's a large data, discard it */
      if ( ins.line_pos + 1 >= ins.len ){
        ins.line_pos = 0;
      }
      ins.data[ins.line_pos++] = ch;
    }

  } else {

    if (!ins.uart_cb) {
      while (uart_getc(&ch)) {}
    } else if (ins.data_len == 0) {
      while (uart_getc(&ch)) {
        ins.uart_cb(&ch, 1);
      }
    } else {
      while (uart_getc(&ch)) {
        ins.data[ins.line_pos++] = ch;
        if( ins.line_pos >= ins.len ||
           (ins.data_len >= 0 && ins.line_pos >= ins.data_len) ||
           (ins.data_len  < 0 && ch == ins.end_char )) {
          ins.uart_cb(ins.data, ins.line_pos);
          ins.line_pos = 0;
        }
      }
    }
  }
  return false;
}

