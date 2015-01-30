
/*
 * custom_at.h
 *
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CUSTOM_AT_H_
#define CUSTOM_AT_H_

#include "c_types.h"

typedef struct
{
  char *at_cmdName;
  int8_t at_cmdLen;
  void (*at_testCmd)(uint8_t id);
  void (*at_queryCmd)(uint8_t id);
  void (*at_setupCmd)(uint8_t id, char *pPara);
  void (*at_exeCmd)(uint8_t id);
}at_funcationType;

/**
  * @brief  Response "OK" to uart.
  * @param  None
  * @retval None
  */
void at_response_ok(void);
/**
  * @brief  Response "ERROR" to uart.
  * @param  events: no used
  * @retval None
  */
void at_response_error(void);
/**
  * @brief  Task of process command or txdata.
  * @param  custom_at_cmd_array: the array of at cmd that custom defined
  *         cmd_num : the num of at cmd that custom defined
  * @retval None
  */
void at_cmd_array_regist(at_funcationType *custom_at_cmd_array,uint32 cmd_num);
/**
  * @brief  get digit form at cmd line.the maybe alter pSrc
  * @param  p_src: at cmd line string
  *         result:the buffer to be placed result
  *         err : err num
  * @retval TRUE:
  *         FALSE:
  */
bool at_get_next_int_dec(char **p_src,int*result,int* err);
/**
  * @brief  get string form at cmd line.the maybe alter pSrc
  * @param  p_dest: the buffer to be placed result
  *         p_src: at cmd line string
  *         max_len :max len of string excepted to get
  * @retval None
  */
int32 at_data_str_copy(char *p_dest, char **p_src, int32 max_len);

/**
  * @brief  initialize at module
  * @param  None
  * @retval None
  */
void at_init(void);
/**
  * @brief  print string to at port
  * @param  string
  * @retval None
  */
void at_port_print(const char *str);
#endif
