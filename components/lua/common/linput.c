#include "driver/console.h"
#include "platform.h"
#include "linput.h"
#include "lua.h"
#include "lauxlib.h"
#include <stdio.h>

static struct input_state {
  char       *data;
  int         line_pos;
  size_t      len;
  const char *prompt;
  char last_nl_char;
} ins = {0};

#define NUL '\0'
#define BS  '\010'
#define CR  '\r'
#define LF  '\n'
#define DEL  0x7f
#define BS_OVER "\010 \010"

static bool input_readline(void);

extern bool uart_on_data_cb(unsigned id, const char *buf, size_t len);
extern bool uart0_echo;
extern bool run_input;
extern uart_status_t uart_status[];

void lua_handle_input(void)
{
  while(input_readline()) {}
}

/*
** The input state (ins) is private, so input_setup() exposes the necessary
** access to public properties and is called in user_init() before the Lua
** enviroment is initialised.
*/
void input_setup(int bufsize, const char *prompt) {
  // Initialise non-zero elements
  ins.data      = malloc(bufsize);
  ins.len       = bufsize;
  ins.prompt    = prompt;
}

void input_setprompt (const char *prompt) {
  ins.prompt = prompt;
}

/*
** input_readline() is called from the lua_handle_input() event routine which
** is called when input data is available. This works in one of two modes
** depending on the bool ins.run_input.
** -  TRUE:   it clears the input buf up to EOL, doing any callback and sending
**            the line to Lua.
** -  FALSE:  it clears the input buf doing callbacks according to the data_len
**            or end_char break.
*/
static bool input_readline(void) {
  char ch = NUL;
  while (console_getc(&ch)) {
    if (run_input) {
      /* handle CR & LF characters and aggregate \n\r and \r\n pairs */
      char tmp_last_nl_char = ins.last_nl_char;
      /* handle CR & LF characters
         filters second char of LF&CR (\n\r) or CR&LF (\r\n) sequences */
      if ((ch == CR && tmp_last_nl_char == LF) || // \n\r sequence -> skip \r
          (ch == LF && tmp_last_nl_char == CR))   // \r\n sequence -> skip \n
      {
        continue;
      }

      /* backspace key */
      if (ch == DEL || ch == BS) {
        if (ins.line_pos > 0) {
          if(uart0_echo) printf(BS_OVER);
          ins.line_pos--;
        }
        ins.data[ins.line_pos] = 0;
        continue;
      }

      /* end of data */
      if (ch == CR || ch == LF) {
        ins.last_nl_char = ch;

        if (uart0_echo) putchar(LF);
        if (ins.line_pos == 0) {
          /* Get a empty line, then go to get a new line */
          printf(ins.prompt);
          fflush(stdout);
          continue;
        } else {
          ins.data[ins.line_pos++] = LF;
          lua_input_string(ins.data, ins.line_pos);
          ins.line_pos = 0;
          return true;
        }
      }
      else
        ins.last_nl_char = NUL;

      if(uart0_echo) putchar(ch);

      /* it's a large line, discard it */
      if ( ins.line_pos + 1 >= ins.len ){
        ins.line_pos = 0;
      }
    }

    ins.data[ins.line_pos++] = ch;

    if (!run_input)
    {
      uart_status_t *us = & uart_status[CONSOLE_UART];
      if(((us->need_len!=0) && (ins.line_pos>= us->need_len)) || \
         (ins.line_pos >= ins.len) || \
         ((us->end_char>=0) && ((unsigned char)ch==(unsigned char)us->end_char)))
      {
        uart_on_data_cb(CONSOLE_UART, ins.data, ins.line_pos);
        ins.line_pos = 0;
      }
    }

  }
  return false;
}


void output_redirect(const char *str, size_t l) {
  lua_State *L = lua_getstate();
  int n = lua_gettop(L);
  lua_pushliteral(L, "stdout");
  lua_rawget(L, LUA_REGISTRYINDEX);                       /* fetch reg.stdout */
  if (lua_istable(L, -1)) { /* reg.stdout is pipe */
    lua_rawgeti(L, -1, 1);          /* get the pipe_write func from stdout[1] */
    lua_insert(L, -2);                         /* and move above the pipe ref */
    lua_pushlstring(L, str, l);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {           /* Reg.stdout:write(str) */
      lua_writestringerror("error calling stdout:write(%s)\n", lua_tostring(L, -1));
      esp_restart();
    }
  } else { /* reg.stdout == nil */
    printf(str, l);
  }
  lua_settop(L, n);         /* Make sure all code paths leave stack unchanged */
}
