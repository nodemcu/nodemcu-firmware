#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "driver/uart.h"
#include "c_types.h"

LOCAL os_timer_t readline_timer;

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;

#define uart_putc uart0_putc

bool uart_getc(char *c){
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

#if 0
int readline4lua(const char *prompt, char *buffer, int length){
    char ch;
    int line_position;

start:
    /* show prompt */
    uart0_sendStr(prompt);

    line_position = 0;
    os_memset(buffer, 0, length);
    while (1)
    {
        while (uart_getc(&ch))
        {
            /* handle CR key */
            if (ch == '\r')
            {
                char next;
                if (uart_getc(&next))
                    ch = next;
            }
            /* backspace key */
            else if (ch == 0x7f || ch == 0x08)
            {
                if (line_position > 0)
                {
                    uart_putc(0x08);
                    uart_putc(' ');
                    uart_putc(0x08);
                    line_position--;
                }
                buffer[line_position] = 0;
                continue;
            }
            /* EOF(ctrl+d) */
            else if (ch == 0x04)
            {
                if (line_position == 0)
                    /* No input which makes lua interpreter close */
                    return 0;
                else
                    continue;
            }
            
            /* end of line */
            if (ch == '\r' || ch == '\n')
            {
                buffer[line_position] = 0;
                uart_putc('\n');
                if (line_position == 0)
                {
                    /* Get a empty line, then go to get a new line */
                    goto start;
                }
                else
                {
                    return line_position;
                }
            }

            /* other control character or not an acsii character */
            if (ch < 0x20 || ch >= 0x80)
            {
                continue;
            }

            /* echo */
            uart_putc(ch);
            buffer[line_position] = ch;
            ch = 0;
            line_position++;

            /* it's a large line, discard it */
            if (line_position >= length)
                line_position = 0;
       }
    }
}
#endif
