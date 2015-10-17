/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: esp_platform_user_timer.c
 *
 * Description:
 *
 * Modification history:
 *     2014/5/09, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
#include "user_esp_platform.h"

#define ESP_DEBUG

#ifdef ESP_DEBUG
#define ESP_DBG os_printf
#else
#define ESP_DBG
#endif

LOCAL os_timer_t device_timer;
uint32 min_wait_second;
char timestamp_str[11];
int timestamp = 0;
char *timer_splits[20] = {NULL};

struct esp_platform_wait_timer_param {
    uint8 wait_time_param[11];
    uint8 wait_action[15];
    int wait_time_second;
};

struct wait_param {
    uint8 action[20][15];
    uint16 action_number;
    uint16 count;
    uint32 min_time_backup;
};

void esp_platform_timer_action(struct esp_platform_wait_timer_param   *timer_wait_param, uint16 count);

/******************************************************************************
 * FunctionName : split
 * Description  : split string p1 according to sting p2 and save the splits
 * Parameters   : p1 , p2 ,splits[]
 * Returns      : the number of splits
*******************************************************************************/
uint16 ICACHE_FLASH_ATTR
split(char *p1, char *p2, char *splits[])
{
    int i = 0;
    int j = 0;

    while (i != -1) {
        int start = i;
        int end = indexof(p1, p2, start);

        if (end == -1) {
            end = os_strlen(p1);
        }

        char *p = (char *) os_zalloc(100);
        os_memcpy(p, p1 + start, end - start);
        p[end - start] = '\0';
        splits[j] = p;
        j++;
        i = end + 1;

        if (i > os_strlen(p1)) {
            break;
        }
    }

    return j;
}

/******************************************************************************
 * FunctionName : indexof
 * Description  : calculate the offset of p2 relate to start of p1
 * Parameters   : p1,p1,start
 * Returns      : the offset of p2 relate to the start
*******************************************************************************/
int ICACHE_FLASH_ATTR
indexof(char *p1, char *p2, int start)
{
    char *find = (char *)os_strstr(p1 + start, p2);

    if (find != NULL) {
        return (find - p1);
    }

    return -1;
}

/******************************************************************************
 * FunctionName : esp_platform_find_min_time
 * Description  : find the minimum wait second in timer list
 * Parameters   : timer_wait_param -- param of timer action and wait time param
 *                count -- The number of timers given by server
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
esp_platform_find_min_time(struct esp_platform_wait_timer_param *timer_wait_param , uint16 count)
{
    uint16 i = 0;
    min_wait_second = 0xFFFFFFF;

    for (i = 0; i < count ; i++) {
        if (timer_wait_param[i].wait_time_second < min_wait_second && timer_wait_param[i].wait_time_second >= 0) {
            min_wait_second = timer_wait_param[i].wait_time_second;
        }
    }
}

/******************************************************************************
 * FunctionName : user_platform_timer_first_start
 * Description  : calculate the wait time of each timer
 * Parameters   : count -- The number of timers given by server
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_platform_timer_first_start(uint16 count)
{
    int i = 0;
    struct esp_platform_wait_timer_param timer_wait_param[100] = {0};

    ESP_DBG("current timestamp= %ds\n", timestamp);

    timestamp = timestamp + min_wait_second;

    for (i = 0 ; i < count ; i++) {
        char *str = timer_splits[i];

        if (indexof(str, "f", 0) == 0) {
            char *fixed_wait[2];

            ESP_DBG("timer is fixed mode\n");

            split(str, "=", fixed_wait);
            os_memcpy(timer_wait_param[i].wait_time_param, fixed_wait[0] + 1, os_strlen(fixed_wait[0]) - 1);
            os_memcpy(timer_wait_param[i].wait_action, fixed_wait[1], os_strlen(fixed_wait[1]));
            timer_wait_param[i].wait_time_second = atoi(timer_wait_param[i].wait_time_param) - timestamp;
            os_free(fixed_wait[0]);
            os_free(fixed_wait[1]);
        }

        else if (indexof(str, "l", 0) == 0) {
            char *loop_wait[2];

            ESP_DBG("timer is loop mode\n");

            split(str, "=", loop_wait);
            os_memcpy(timer_wait_param[i].wait_time_param, loop_wait[0] + 1, os_strlen(loop_wait[0]) - 1);
            os_memcpy(timer_wait_param[i].wait_action, loop_wait[1], os_strlen(loop_wait[1]));
            timer_wait_param[i].wait_time_second = atoi(timer_wait_param[i].wait_time_param) - (timestamp % atoi(timer_wait_param[i].wait_time_param));
            os_free(loop_wait[0]);
            os_free(loop_wait[1]);
        } else if (indexof(str, "w", 0) == 0) {
            char *week_wait[2];
            int monday_wait_time = 0;

            ESP_DBG("timer is weekend mode\n");

            split(str, "=", week_wait);
            os_memcpy(timer_wait_param[i].wait_time_param, week_wait[0] + 1, os_strlen(week_wait[0]) - 1);
            os_memcpy(timer_wait_param[i].wait_action, week_wait[1], os_strlen(week_wait[1]));
            monday_wait_time = (timestamp - 1388937600) % (7 * 24 * 3600);

            ESP_DBG("monday_wait_time == %d", monday_wait_time);

            if (atoi(timer_wait_param[i].wait_time_param) > monday_wait_time) {
                timer_wait_param[i].wait_time_second = atoi(timer_wait_param[i].wait_time_param) - monday_wait_time;
            } else {
                timer_wait_param[i].wait_time_second = 7 * 24 * 3600 - monday_wait_time + atoi(timer_wait_param[i].wait_time_param);
            }

            os_free(week_wait[0]);
            os_free(week_wait[1]);
        }
    }

    esp_platform_find_min_time(timer_wait_param, count);
    if(min_wait_second == 0) {
    	return;
    }

    esp_platform_timer_action(timer_wait_param, count);
}

/******************************************************************************
 * FunctionName : user_esp_platform_device_action
 * Description  : Execute the actions of minimum wait time
 * Parameters   : pwait_action -- point the list of actions which need execute
 *
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_device_action(struct wait_param *pwait_action)
{
    uint8 i = 0;
    uint16 count = pwait_action->count;
    uint16 action_number = pwait_action->action_number;

    ESP_DBG("there is %d action at the same time\n", pwait_action->action_number);

#if PLUG_DEVICE
    for (i = 0; i < action_number && pwait_action->action[i][0] != '0'; i++) {
        ESP_DBG("%s\n",pwait_action->action[i]);

        if (os_strcmp(pwait_action->action[i], "on_switch", 9) == 0) {
            user_plug_set_status(0x01);
        } else if (os_strcmp(pwait_action->action[i], "off_switch", 10) == 0) {
            user_plug_set_status(0x00);
        } else if (os_strcmp(pwait_action->action[i], "on_off_switch", 13) == 0) {
            if (user_plug_get_status() == 0) {
                user_plug_set_status(0x01);
            } else {
                user_plug_set_status(0x00);
            }
        } else {
            return;
        }
    }
    user_platform_timer_first_start(count);
#endif
}

/******************************************************************************
 * FunctionName : user_platform_timer_start
 * Description  : Processing the message about timer from the server
 * Parameters   : timer_wait_param -- The received data from the server
 *                count -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_wait_time_overflow_check(struct wait_param *pwait_action)
{
    ESP_DBG("min_wait_second = %d", min_wait_second);

    if (pwait_action->min_time_backup >= 3600) {
        os_timer_disarm(&device_timer);
        os_timer_setfn(&device_timer, (os_timer_func_t *)user_esp_platform_wait_time_overflow_check, pwait_action);
        os_timer_arm(&device_timer, 3600000, 0);
        ESP_DBG("min_wait_second is extended\n");
    } else {
        os_timer_disarm(&device_timer);
        os_timer_setfn(&device_timer, (os_timer_func_t *)user_esp_platform_device_action, pwait_action);
        os_timer_arm(&device_timer, pwait_action->min_time_backup * 1000, 0);
        ESP_DBG("min_wait_second is = %dms\n", pwait_action->min_time_backup * 1000);
    }

    pwait_action->min_time_backup -= 3600;
}

/******************************************************************************
 * FunctionName : user_platform_timer_start
 * Description  : Processing the message about timer from the server
 * Parameters   : timer_wait_param -- The received data from the server
 *                count -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
esp_platform_timer_action(struct esp_platform_wait_timer_param *timer_wait_param, uint16 count)
{
    uint16 i = 0;
    uint16 action_number;
    struct wait_param pwait_action = {0};

    pwait_action.count = count;
    action_number = 0;

    for (i = 0; i < count ; i++) {
        if (timer_wait_param[i].wait_time_second == min_wait_second) {
            os_memcpy(pwait_action.action[action_number], timer_wait_param[i].wait_action, os_strlen(timer_wait_param[i].wait_action));
            ESP_DBG("*****%s*****\n", timer_wait_param[i].wait_action);
            action_number++;
        }
    }

    pwait_action.action_number = action_number;
    pwait_action.min_time_backup = min_wait_second;
    user_esp_platform_wait_time_overflow_check(&pwait_action);
}

/******************************************************************************
 * FunctionName : user_platform_timer_start
 * Description  : Processing the message about timer from the server
 * Parameters   : pbuffer -- The received data from the server

 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_platform_timer_start(char *pbuffer)
{
    int str_begin = 0;
    int str_end = 0;
    uint8 i = 0;
    char *pstr_start = NULL;
    char *pstr_end = NULL;
    struct esp_platform_wait_timer_param timer_wait_param[20];
    char *pstr = NULL;

    min_wait_second  = 0;

    if ((pstr = (char *)os_strstr(pbuffer, "\"timestamp\":")) != NULL) {
        pstr_start = pstr + 13;
        pstr_end = (char *)os_strstr(pstr_start, ",");

        if (pstr != NULL) {
            os_memcpy(timestamp_str, pstr_start, pstr_end - pstr_start);
            timestamp = atoi(timestamp_str);
        }
    }

    for (i = 0 ; i < 20 ; i++) {
        if (timer_splits[i] != NULL) {
            os_free(timer_splits[i]);
            timer_splits[i] = NULL;
        }
    }

    if ((pstr_start = (char *)os_strstr(pbuffer, "\"timers\": \"")) != NULL) {
        str_begin = 11;
        str_end = indexof(pstr_start, "\"", str_begin);

        if (str_begin == str_end) {
        	os_timer_disarm(&device_timer);
            return;
        }

        char *split_buffer = (char *)os_zalloc(str_end - str_begin + 1);
        os_memcpy(split_buffer, pstr_start + str_begin, str_end - str_begin);
        uint16 count = split(split_buffer , ";" , timer_splits);
        os_free(split_buffer);
        user_platform_timer_first_start(count);
    }
}
