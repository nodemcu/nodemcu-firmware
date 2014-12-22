#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/mem.h"
#include "lwip/app/espconn.h"

#include "upgrade.h"

#include "upgrade_lib.c"

#define UPGRADE_DEBUG
#ifdef UPGRADE_DEBUG
#define UPGRADE_DBG os_printf
#else
#define UPGRADE_DBG
#endif

LOCAL struct espconn *upgrade_conn;
LOCAL uint8 *pbuf;
LOCAL os_timer_t upgrade_10s;
LOCAL os_timer_t upgrade_timer;
LOCAL uint32 totallength = 0;
LOCAL uint32 sumlength = 0;

/******************************************************************************
 * FunctionName : upgrade_disconcb
 * Description  : The connection has been disconnected successfully.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
upgrade_disconcb(void *arg)
{
    struct espconn *pespconn = arg;

    if (pespconn == NULL) {
        return;
    }

    os_free(pespconn->proto.tcp);
    pespconn->proto.tcp = NULL;
    os_free(pespconn);
    pespconn = NULL;
    upgrade_conn = NULL;
}

/******************************************************************************
 * FunctionName : upgrade_datasent
 * Description  : Data has been sent successfully,This means that more data can
 *                be sent.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
upgrade_datasent(void *arg)
{
    struct espconn *pespconn = arg;

    if (pespconn ->state == ESPCONN_CONNECT) {
    }
}

/******************************************************************************
 * FunctionName : upgrade_deinit
 * Description  : disconnect the connection with the host
 * Parameters   : bin -- server number
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
LOCAL upgrade_deinit(void)
{
    if (system_upgrade_flag_check() != UPGRADE_FLAG_START) {
        system_upgrade_deinit();
    }


}

/******************************************************************************
 * FunctionName : upgrade_10s_cb
 * Description  : Processing the client when connected with host time out
 * Parameters   : pespconn -- A point to the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR upgrade_10s_cb(struct espconn *pespconn)
{
    if (pespconn == NULL) {
        return;
    }

    system_upgrade_deinit();
    os_free(pespconn->proto.tcp);
    pespconn->proto.tcp = NULL;
    os_free(pespconn);
    pespconn = NULL;
    upgrade_conn = NULL;
}

/******************************************************************************
 * FunctionName : user_upgrade_check
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
upgrade_check(struct upgrade_server_info *server)
{
	UPGRADE_DBG("upgrade_check\n");

    if (system_upgrade_flag_check() != UPGRADE_FLAG_FINISH) {
    	totallength = 0;
    	sumlength = 0;
        os_timer_disarm(&upgrade_timer);
        system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
        upgrade_deinit();
        server->upgrade_flag = false;

        if (server->check_cb != NULL) {
            server->check_cb(server);
        }
    } else {
        os_timer_disarm(&upgrade_timer);
        upgrade_deinit();
        server->upgrade_flag = true;

        if (server->check_cb != NULL) {
            server->check_cb(server);
        }
    }
#ifdef UPGRADE_SSL_ENABLE
    espconn_secure_disconnect(upgrade_conn);
#else
    espconn_disconnect(upgrade_conn);

#endif
}

/******************************************************************************
 * FunctionName : upgrade_download
 * Description  : Processing the upgrade data from the host
 * Parameters   : bin -- server number
 *                pusrdata -- The upgrade data (or NULL when the connection has been closed!)
 *                length -- The length of upgrade data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
upgrade_download(void *arg, char *pusrdata, unsigned short length)
{
    char *ptr = NULL;
    char *ptmp2 = NULL;
    char lengthbuffer[32];
    if (totallength == 0 && (ptr = (char *)os_strstr(pusrdata, "\r\n\r\n")) != NULL &&
            (ptr = (char *)os_strstr(pusrdata, "Content-Length")) != NULL) {
        ptr = (char *)os_strstr(pusrdata, "\r\n\r\n");
        length -= ptr - pusrdata;
        length -= 4;
        totallength += length;
        UPGRADE_DBG("upgrade file download start.\n");
        system_upgrade(ptr + 4, length);
        ptr = (char *)os_strstr(pusrdata, "Content-Length: ");

        if (ptr != NULL) {
            ptr += 16;
            ptmp2 = (char *)os_strstr(ptr, "\r\n");

            if (ptmp2 != NULL) {
                os_memset(lengthbuffer, 0, sizeof(lengthbuffer));
                os_memcpy(lengthbuffer, ptr, ptmp2 - ptr);
                sumlength = atoi(lengthbuffer);
            } else {
                UPGRADE_DBG("sumlength failed\n");
            }
        } else {
            UPGRADE_DBG("Content-Length: failed\n");
        }
    } else {
        totallength += length;
        os_printf("totallen = %d\n",totallength);
        system_upgrade(pusrdata, length);
    }

    if (totallength == sumlength) {
        UPGRADE_DBG("upgrade file download finished.\n");
        system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
        totallength = 0;
        sumlength = 0;
        upgrade_check(upgrade_conn->reverse);
        os_timer_disarm(&upgrade_10s);
        os_timer_setfn(&upgrade_10s, (os_timer_func_t *)upgrade_deinit, NULL);
        os_timer_arm(&upgrade_10s, 10, 0);
    } else {
        if (upgrade_conn->state != ESPCONN_READ) {
            totallength = 0;
            sumlength = 0;
            os_timer_disarm(&upgrade_10s);
            os_timer_setfn(&upgrade_10s, (os_timer_func_t *)upgrade_check, upgrade_conn->reverse);
            os_timer_arm(&upgrade_10s, 10, 0);
        }
    }
}

/******************************************************************************
 * FunctionName : upgrade_connect
 * Description  : client connected with a host successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
upgrade_connect_cb(void *arg)
{
    struct espconn *pespconn = arg;

    UPGRADE_DBG("upgrade_connect_cb\n");
    os_timer_disarm(&upgrade_10s);

    espconn_regist_disconcb(pespconn, upgrade_disconcb);
    espconn_regist_sentcb(pespconn, upgrade_datasent);

    if (pbuf != NULL) {
        UPGRADE_DBG(pbuf);
#ifdef UPGRADE_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif
    }
}

/******************************************************************************
 * FunctionName : upgrade_connection
 * Description  : connect with a server
 * Parameters   : bin -- server number
 *                url -- the url whitch upgrade files saved
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
upgrade_connect(struct upgrade_server_info *server)
{
	UPGRADE_DBG("upgrade_connect\n");

	pbuf = server->url;

    espconn_regist_connectcb(upgrade_conn, upgrade_connect_cb);
    espconn_regist_recvcb(upgrade_conn, upgrade_download);

    system_upgrade_init();
    system_upgrade_flag_set(UPGRADE_FLAG_START);

#ifdef UPGRADE_SSL_ENABLE
    espconn_secure_connect(upgrade_conn);
#else
    espconn_connect(upgrade_conn);
#endif

    os_timer_disarm(&upgrade_10s);
    os_timer_setfn(&upgrade_10s, (os_timer_func_t *)upgrade_10s_cb, upgrade_conn);
    os_timer_arm(&upgrade_10s, 10000, 0);
}

/******************************************************************************
 * FunctionName : user_upgrade_init
 * Description  : parameter initialize as a client
 * Parameters   : server -- A point to a server parmer which connected
 * Returns      : none
*******************************************************************************/
bool ICACHE_FLASH_ATTR
#ifdef UPGRADE_SSL_ENABLE
system_upgrade_start_ssl(struct upgrade_server_info *server)
#else
system_upgrade_start(struct upgrade_server_info *server)
#endif
{
    if (system_upgrade_flag_check() == UPGRADE_FLAG_START) {
        return false;
    }
    if (server == NULL) {
    	UPGRADE_DBG("server is NULL\n");
    	return false;
    }
    if (upgrade_conn == NULL) {
        upgrade_conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
    }

    if (upgrade_conn != NULL) {
        upgrade_conn->proto.tcp = NULL;
        upgrade_conn->type = ESPCONN_TCP;
        upgrade_conn->state = ESPCONN_NONE;
        upgrade_conn->reverse = server;

        if (upgrade_conn->proto.tcp == NULL) {
            upgrade_conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
        }

        if (upgrade_conn->proto.tcp != NULL) {
            upgrade_conn->proto.tcp->local_port = espconn_port();
            upgrade_conn->proto.tcp->remote_port = server->port;

            os_memcpy(upgrade_conn->proto.tcp->remote_ip, server->ip, 4);

            UPGRADE_DBG("%s\n", __func__);
            upgrade_connect(server);

            if (server->check_cb !=  NULL) {
                os_timer_disarm(&upgrade_timer);
                os_timer_setfn(&upgrade_timer, (os_timer_func_t *)upgrade_check, server);
                os_timer_arm(&upgrade_timer, server->check_times, 0);
            }
        }
    }

    return true;
}
