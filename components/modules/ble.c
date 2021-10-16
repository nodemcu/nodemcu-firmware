/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "sdkconfig.h"
#ifdef CONFIG_NODEMCU_CMODULE_BLE
#include "nvs_flash.h"
#include <assert.h>
#include <errno.h>

#include "module.h"
#include "lauxlib.h"
#include "task/task.h"
#include "platform.h"
#include "esp_bt.h"
#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#define TAG "ble"

/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/ble.h"
#include "task/task.h"

#define PERROR() printf("pcall failed: %s\n", lua_tostring(L, -1))

static int lble_start_advertising();
static int lble_gap_event(struct ble_gap_event *event, void *arg);

static const char *gadget_name;

static const struct ble_gatt_svc_def *gatt_svr_svcs;

static task_handle_t task_handle;
static QueueHandle_t response_queue;

static int struct_pack_index;
static int struct_unpack_index;

static int seqno;

// Note that the buffer should be freed 
typedef struct {
  int seqno;
  int errcode;
  char *buffer;
  size_t length;
} response_message_t;

typedef struct {
  int seqno;
  struct ble_gatt_access_ctxt *ctxt;
  void *arg;
  char *buffer;
  size_t length;
} task_block_t;

#undef MODLOG_DFLT
#define MODLOG_DFLT(level, ...) printf(__VA_ARGS__)

/**
 * Utility function to log an array of bytes.
 */
static void
print_bytes(const uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        MODLOG_DFLT(INFO, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

static void
print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    MODLOG_DFLT(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}



#if 0


#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp/bsp.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "bleprph.h"

/**
 * The vendor specific security test service consists of two characteristics:
 *     o random-number-generator: generates a random 32-bit number each time
 *       it is read.  This characteristic can only be read over an encrypted
 *       connection.
 *     o static-value: a single-byte characteristic that can always be read,
 *       but can only be written over an encrypted connection.
 */

/* 59462f12-9543-9999-12c8-58b459a2712d */
static const ble_uuid128_t gatt_svr_svc_sec_test_uuid =
    BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                     0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/* 5c3a659e-897e-45e1-b016-007107c96df6 */
static const ble_uuid128_t gatt_svr_chr_sec_test_rand_uuid =
        BLE_UUID128_INIT(0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                         0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

/* 5c3a659e-897e-45e1-b016-007107c96df7 */
static const ble_uuid128_t gatt_svr_chr_sec_test_static_uuid =
        BLE_UUID128_INIT(0xf7, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                         0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

static uint8_t gatt_svr_sec_test_static_val;

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Security test. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_sec_test_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Random number generator. */
            .uuid = &gatt_svr_chr_sec_test_rand_uuid.u,
            .access_cb = gatt_svr_chr_access_sec_test,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
        }, {
            /*** Characteristic: Static value. */
            .uuid = &gatt_svr_chr_sec_test_static_uuid.u,
            .access_cb = gatt_svr_chr_access_sec_test,
            .flags = BLE_GATT_CHR_F_READ |
                     BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const ble_uuid_t *uuid;
    int rand_num;
    int rc;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_rand_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a 32-bit random number. */
        rand_num = rand();
        rc = os_mbuf_append(ctxt->om, &rand_num, sizeof rand_num);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_static_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            rc = os_mbuf_append(ctxt->om, &gatt_svr_sec_test_static_val,
                                sizeof gatt_svr_sec_test_static_val);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof gatt_svr_sec_test_static_val,
                                    sizeof gatt_svr_sec_test_static_val,
                                    &gatt_svr_sec_test_static_val, NULL);
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}


/**
 * Logs information about a connection to the console.
 */
static void
bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
        }
        MODLOG_DFLT(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        bleprph_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(tag, "PASSKEY_ACTION_EVENT started \n");
        struct ble_sm_io pkey = {0};
        int key = 0;

        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            pkey.action = event->passkey.params.action;
            pkey.passkey = 123456; // This is the passkey to be entered on peer
            ESP_LOGI(tag, "Enter passkey %d on the peer side", pkey.passkey);
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(tag, "ble_sm_inject_io result: %d\n", rc);
        } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            ESP_LOGI(tag, "Passkey on device's display: %d", event->passkey.params.numcmp);
            ESP_LOGI(tag, "Accept or reject the passkey through console in this format -> key Y or key N");
            pkey.action = event->passkey.params.action;
            if (scli_receive_key(&key)) {
                pkey.numcmp_accept = key;
            } else {
                pkey.numcmp_accept = 0;
                ESP_LOGE(tag, "Timeout! Rejecting the key");
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(tag, "ble_sm_inject_io result: %d\n", rc);
        } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
            static uint8_t tem_oob[16] = {0};
            pkey.action = event->passkey.params.action;
            for (int i = 0; i < 16; i++) {
                pkey.oob[i] = tem_oob[i];
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(tag, "ble_sm_inject_io result: %d\n", rc);
        } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
            ESP_LOGI(tag, "Enter the passkey through console in this format-> key 123456");
            pkey.action = event->passkey.params.action;
            if (scli_receive_key(&key)) {
                pkey.passkey = key;
            } else {
                pkey.passkey = 0;
                ESP_LOGE(tag, "Timeout! Passing 0 as the key");
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(tag, "ble_sm_inject_io result: %d\n", rc);
        }
        return 0;
    }

    return 0;
}

static void
bleprph_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
bleprph_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");
    /* Begin advertising. */
    bleprph_advertise();
}

void bleprph_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void
app_main(void)
{
    int rc;

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

    nimble_port_init();
    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = CONFIG_EXAMPLE_IO_TYPE;
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif
#ifdef CONFIG_EXAMPLE_MITM
    ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_EXAMPLE_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0;
#endif
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;
#endif


    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("nimble-bleprph");
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(bleprph_host_task);

    /* Initialize command line interface to accept input from user */
    rc = scli_init();
    if (rc != ESP_OK) {
        ESP_LOGE(tag, "scli_init() failed");
    }
}
#endif
static int
gethexval(char c) {
  if (c < '0') {
    return -1;
  }
  if (c <= '9') {
    return c - '0';
  }
  if (c < 'a') {
    c += 'a' - 'A';
  }
  if (c < 'a') {
    return -1;
  }
  if (c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

static int 
decodehex(const char *s) {
  // two characters
  int v1 = gethexval(s[0]);
  int v2 = gethexval(s[1]);

  if (v1 < 0 || v2 < 0) {
    return -1;
  }
  return (v1 << 4) + v2;
}

static bool
convert_uuid(ble_uuid_any_t *uuid, const char *s) {
  int len = strlen(s);
  char decodebuf[32];

  const char *sptr = s + len;
  int i;

  for (i = 0; sptr > s && i < sizeof(decodebuf); ) {
    if (sptr < s + 2) {
      return false;
    }
    if (sptr[-1] == '-') {
      sptr--;
      continue;
    }
    sptr -= 2;
    
    int val = decodehex(sptr);
    if (val < 0) {
      return false;
    }
    decodebuf[i++] = val;
  }

  if (sptr != s) {
    return false;
  }

  if (ble_uuid_init_from_buf(uuid, decodebuf, i) != 0) {
    return false;
  }

  return true;
}

static void
free_gatt_svcs(lua_State *L, const struct ble_gatt_svc_def * svcs) {
  void *tofree = (void *) svcs;
  // Need to unref anything
  if (!svcs) {
    return;
  }

  for (; svcs->characteristics; svcs++) {
    const struct ble_gatt_chr_def *chrs = svcs->characteristics;
    for (; chrs->arg; chrs++) {
      luaL_unref(L, LUA_REGISTRYINDEX, (int) chrs->arg);
    }
  }

  free(tofree);
}

static int
lble_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  // Actually the only thing we care about is the arg and the ctxt

  printf("access_cb called with op %d\n", ctxt->op);

  size_t task_block_size = sizeof(task_block_t);

  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    task_block_size += OS_MBUF_PKTLEN(ctxt->om);
  }

  task_block_t *task_block = malloc(task_block_size);
  if (!task_block) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  memset(task_block, 0, sizeof(*task_block));

  task_block->ctxt = ctxt;
  task_block->arg = arg;
  task_block->seqno = seqno++;
  task_block->buffer = (char *) (task_block + 1);

  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    task_block->length = OS_MBUF_PKTLEN(ctxt->om);
    uint16_t outlen;
    if (ble_hs_mbuf_to_flat(ctxt->om, task_block->buffer, task_block->length, &outlen)) {
      return BLE_ATT_ERR_UNLIKELY;
    }
  }

  printf("ABout to task_post\n");
  
  if (!task_post(TASK_PRIORITY_HIGH, task_handle, (task_param_t) task_block)) {
    free(task_block);
    return BLE_ATT_ERR_UNLIKELY;
  }

  response_message_t message;

  while (1) {
    printf("About to receive\n");

    if (xQueueReceive(response_queue, &message, (TickType_t) (10000/portTICK_PERIOD_MS) ) != pdPASS) {
      free(task_block);
      return BLE_ATT_ERR_UNLIKELY;
    }

    if (message.seqno == task_block->seqno) {
      break;
    }
  }

  int rc = BLE_ATT_ERR_UNLIKELY;

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    rc = message.errcode;
    if (rc == 0) {
      if (os_mbuf_append(ctxt->om, message.buffer, message.length)) {
	rc = BLE_ATT_ERR_INSUFFICIENT_RES;
      }
    }
  } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    rc = message.errcode;
  }

  free(message.buffer);
  free(task_block);

  return rc;
}

static void 
lble_task_cb(task_param_t param, task_prio_t prio) {
  task_block_t *task_block = (task_block_t *) param;

  response_message_t message;
  memset(&message, 0, sizeof(message));
  message.errcode = BLE_ATT_ERR_UNLIKELY;

  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, (int) task_block->arg);
  // Now we have the characteristic table in -1
  lua_getfield(L, -1, "type");
  // -1 is the struct mapping (if any), -2 is the table

  if (task_block->ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    // if there is a read method, then invoke it
    lua_getfield(L, -2, "read");
    if (!lua_isnoneornil (L, -1)) {
      lua_pushvalue(L, -3);   // dup the table onto the top
      if (lua_pcall(L, 1, 1, 0)) {
        // error.
        PERROR();
        message.errcode = BLE_ATT_ERR_UNLIKELY;
        lua_pop(L, 1);
        goto cleanup;
      }
    } else {
      lua_pop(L, 1);  // get rid of the null
      lua_getfield(L, -2, "value");
    }
    // Now we have the value (-1), struct (-2), table (-3)
    if (!lua_isnoneornil(L, -2)) {
      // need to convert value
      printf("About to convert vaclue for read\n");
      if (!lua_istable(L, -1)) {
        // wrap it in a table
        lua_createtable(L, 1, 0);
        lua_pushvalue(L, -2);  // Now have value, table, value, struct, table
        lua_rawseti(L, -2, 1); 
        lua_remove(L, -2);   // now have table, struct, chr table
        printf("wrapped in table\n");
      }

      // Now call struct.pack
      // The arguments are the format string, and then the values
      lua_rawgeti(L, LUA_REGISTRYINDEX, struct_pack_index);
      lua_pushvalue(L, -3);   // dup the format
      int nv = lua_rawlen(L, -3);
      for (int i = 1; i <= nv; i++) {
        lua_rawgeti(L, -2 - i, i);
      }
      if (lua_pcall(L, nv + 1, 1, 0)) {
        PERROR();
        message.errcode = BLE_ATT_ERR_UNLIKELY;
        lua_pop(L, 2);
        goto cleanup;
      }
      lua_remove(L, -2);   // remove the old value
      // now have string (-1), struct(-2), chrtable (-3)
    }
    size_t datalen;
    const char *data = lua_tolstring(L, -1, &datalen);

    if (data) {
      message.buffer = malloc(datalen);
      if (message.buffer) {
	message.length = datalen;
	memcpy(message.buffer, data, datalen);
	message.errcode = 0;
      }
    }
    lua_pop(L, 1);
    message.errcode = 0;
  }
  if (task_block->ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    // Push the value
    lua_pushlstring(L, task_block->buffer, task_block->length);

    // value, struct, chrtable
    // If we have a struct, then unpack the values
    if (!lua_isnoneornil(L, -2)) {
      lua_createtable(L, 0, 0);
      int stack_size = lua_gettop(L);
      lua_rawgeti(L, LUA_REGISTRYINDEX, struct_unpack_index);
      lua_pushvalue(L, -4);   // dup the format
      lua_pushvalue(L, -4);   // dup the string
      if (lua_pcall(L, 2, LUA_MULTRET, 0)) {
        PERROR();
        message.errcode = BLE_ATT_ERR_UNLIKELY;
        lua_pop(L, 2);
        goto cleanup;
      }
      int vals = lua_gettop(L) - stack_size;
      printf("unpacked %d vals\n", vals - 1);

      // Note that the last entry is actually the string offset
      for (int i = 1; i < vals; i++) {
        lua_pushvalue(L, -(vals - i + 1));
        lua_rawseti(L, -(vals + 2), i);
      }
      lua_pop(L, vals);
      lua_remove(L, -2);
      // Now have table, struct, chrtable 
    }
    // If the value is a table of a single value, then
    // treat as value
    if (lua_istable(L, -1) && lua_rawlen(L, -1) == 1) {
      lua_rawgeti(L, -1, 1);
      lua_remove(L, -2);	// and throw away the table
    }
    lua_getfield(L, -3, "write");
    if (!lua_isnoneornil(L, -1)) {
      lua_pushvalue(L, -2);   // the values table
      if (lua_pcall(L, 1, 0, 0)) {
        PERROR();
	message.errcode = BLE_ATT_ERR_UNLIKELY;
	lua_pop(L, 2);
	goto cleanup;
      }
    }
    lua_pop(L, 1);  // Throw away the null write pointer
    // just save the result in the value
    lua_setfield(L, -3, "value");
    message.errcode = 0;
  }

cleanup:
  printf("Returning code %d\n", message.errcode);
  lua_pop(L, 2);
  message.seqno = task_block->seqno;

  xQueueSend(response_queue, &message, (TickType_t) 0);
}

static int 
lble_build_gatt_svcs(lua_State *L, struct ble_gatt_svc_def **resultp) {
  // We have to first figure out how big the allocated memory is.
  // This is the number of services (ns) + 1 * sizeof(ble_gatt_svc_def)
  // + number of characteristics (nc) + ns * sizeof(ble_gatt_chr_def)
  // + ns + nc * sizeof(ble_uuid_any_t)

  printf("build_gatt_svcs\n");

  lua_getfield(L, 1, "services");
  if (!lua_istable(L, -1)) {
    return luaL_error(L, "services entry must be a table");
  }
  int ns = lua_rawlen(L, -1);

  // -1 is the services list
  int nc = 0;
  for (int i = 1; i <= ns; i++) {
    lua_geti(L, -1, i);
    // -1 is now the service which should be a table. It must have a uuid
    if (lua_type(L, -1) != LUA_TTABLE) {
      luaL_error(L, "The services list must contain tables");
    }
    if (lua_getfield(L, -1, "uuid") != LUA_TSTRING) {
      luaL_error(L, "The service uuid must be a string");
    }
    lua_pop(L, 1);
    // -1 is the service again
    lua_getfield(L, -1, "characteristics");
    nc += lua_rawlen(L, -1);
    lua_pop(L, 2);
  }

  int size = (ns + 1) * sizeof(struct ble_gatt_svc_def) + (nc + ns) * sizeof(struct ble_gatt_chr_def) + (ns + nc) * sizeof(ble_uuid_any_t);

  printf("Computed size: %d (nc %d, ns %d)\n", size, nc, ns);

  struct ble_gatt_svc_def *svcs = malloc(size);
  if (!svcs) {
    luaL_error(L, "Unable to allocate memory: %d", size);
  }

  memset(svcs, 0, size);
  struct ble_gatt_svc_def *result = svcs;
  void *eom = ((char *) svcs) + size;
  struct ble_gatt_chr_def *chrs = (struct ble_gatt_chr_def *) (svcs + ns + 1);
  ble_uuid_any_t *uuids = (ble_uuid_any_t *) (chrs + ns + nc);

  // Now fill out the data structure
  // -1 is the services list
  for (int i = 1; i <= ns; i++) {
    struct ble_gatt_svc_def *svc = svcs++;
    lua_geti(L, -1, i);
    // -1 is now the service which should be a table. It must have a uuid
    lua_getfield(L, -1, "uuid");
    // Convert the uuid
    if ((void *) (uuids + 1) > eom) {
      free_gatt_svcs(L, result);
      return luaL_error(L, "Miscalculated memory requirements");
    }
    if (!convert_uuid(uuids, lua_tostring(L, -1))) {
      free_gatt_svcs(L, result);
      return luaL_error(L, "Unable to convert UUID: %s", lua_tostring(L, -1));
    }
    if (i == 1) {
      svc->type = BLE_GATT_SVC_TYPE_PRIMARY;
    }
    svc->uuid = (ble_uuid_t *) uuids++;
    svc->characteristics = chrs;
    lua_pop(L, 1);
    // -1 is the service again
    lua_getfield(L, -1, "characteristics");
    int nc = lua_rawlen(L, -1);
    for (int j = 1; j <= nc; j++) {
      struct ble_gatt_chr_def *chr = chrs++;
      lua_geti(L, -1, j);

      // -1 is now the characteristic
      lua_getfield(L, -1, "uuid");
      // Convert the uuid
      if ((void *) (uuids + 1) > eom) {
	free_gatt_svcs(L, result);
	return luaL_error(L, "Miscalculated memory requirements");
      }
      if (!convert_uuid(uuids, lua_tostring(L, -1))) {
	free_gatt_svcs(L, result);
	return luaL_error(L, "Unable to convert UUID: %s", lua_tostring(L, -1));
      }
      chr->uuid = (ble_uuid_t *) uuids++;
      lua_pop(L, 1);   // pop off the uuid

      if (lua_getfield(L, -1, "value") != LUA_TNIL) {
        chr->flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE;
        lua_pop(L, 1); // pop off value
      } else {
        lua_getfield(L, -1, "read");
	if (!lua_isnoneornil (L, -1)) {
	  luaL_checkfunction (L, -1);
	  chr->flags |= BLE_GATT_CHR_F_READ;
	}

        lua_getfield(L, -1, "write");
	if (!lua_isnoneornil (L, -1)) {
	  luaL_checkfunction (L, -1);
	  chr->flags |= BLE_GATT_CHR_F_WRITE;
	}

        lua_pop(L, 3); // pop off value, read, write
      }

      // -1 is now the characteristic again
      chr->arg = (void *) luaL_ref(L, LUA_REGISTRYINDEX);
      chr->access_cb = lble_access_cb;
    }
    lua_pop(L, 2);
  }
  lua_pop(L, 1);

  *resultp = result;
 
  return 0;
}

static int
gatt_svr_init(lua_State *L) {
    int rc;

    printf("about to call gap_init\n");

    ble_svc_gap_init();
    printf("about to call gatt_init\n");
    ble_svc_gatt_init();
    
    // Now we have to build the gatt_svr_svcs data structure

    
    struct ble_gatt_svc_def *svcs = NULL;
    lble_build_gatt_svcs(L, &svcs);
    free_gatt_svcs(L, gatt_svr_svcs);
    gatt_svr_svcs = svcs;
    

    printf("about to call count_cfg\n");
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
      return luaL_error(L, "Failed to count gatts: %d", rc);
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
      return luaL_error(L, "Failed to add gatts: %d", rc);
    }

    ble_gatts_start();

    return 0;
}

/**
 * Logs information about a connection to the console.
 */
static void
lble_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

static int
lble_sys_init(lua_State *L) {
  int rc = nvs_flash_init();
  if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    rc = nvs_flash_erase();
    if (rc) {
      return luaL_error(L, "Failed to erase flash: %d", rc);
    }
    rc = nvs_flash_init();
  }
  if (rc) {
    return luaL_error(L, "Failed to init flash: %d", rc);
  }

  task_handle = task_get_id(lble_task_cb);
  response_queue = xQueueCreate(2, sizeof(response_message_t));

  return 0;
}

static void 
lble_host_task(void *param)
{
     nimble_port_run(); //This function will return only when nimble_port_stop() is executed.
     nimble_port_freertos_deinit();
}

static void 
lble_init_stack(lua_State *L) {
  int ret = esp_nimble_hci_and_controller_init();
  if (ret != ESP_OK) {
    luaL_error(L, "esp_nimble_hci_and_controller_init() failed with error: %d", ret);
    return;
  }

  nimble_port_init();

  //Initialize the NimBLE Host configuration

  nimble_port_freertos_init(lble_host_task);
}

static int
lble_gap_event(struct ble_gap_event *event, void *arg)
{
  printf("GAP event %d\n", event->type);
  struct ble_gap_conn_desc desc;
  int rc;

  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
      /* A new connection was established or a connection attempt failed. */
      printf("connection %s; status=%d ",
		  event->connect.status == 0 ? "established" : "failed",
		  event->connect.status);
      if (event->connect.status == 0) {
	  rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
	  assert(rc == 0);
	  lble_print_conn_desc(&desc);

      }
      printf("\n");

      if (event->connect.status != 0) {
	  /* Connection failed; resume advertising. */
	  lble_start_advertising();
      }
      return 0;

  case BLE_GAP_EVENT_DISCONNECT:
      printf("disconnect; reason=%d ", event->disconnect.reason);
      lble_print_conn_desc(&event->disconnect.conn);
      printf("\n");

      /* Connection terminated; resume advertising. */
      lble_start_advertising();
      return 0;

  case BLE_GAP_EVENT_CONN_UPDATE:
      /* The central has updated the connection parameters. */
      MODLOG_DFLT(INFO, "connection updated; status=%d ",
		  event->conn_update.status);
      rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
      assert(rc == 0);
      lble_print_conn_desc(&desc);
      MODLOG_DFLT(INFO, "\n");
      return 0;

  case BLE_GAP_EVENT_ADV_COMPLETE:
      MODLOG_DFLT(INFO, "advertise complete; reason=%d",
		  event->adv_complete.reason);
      lble_start_advertising();
      return 0;

  case BLE_GAP_EVENT_ENC_CHANGE:
      /* Encryption has been enabled or disabled for this connection. */
      MODLOG_DFLT(INFO, "encryption change event; status=%d ",
		  event->enc_change.status);
      rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
      assert(rc == 0);
      lble_print_conn_desc(&desc);
      MODLOG_DFLT(INFO, "\n");
      return 0;

  case BLE_GAP_EVENT_SUBSCRIBE:
      MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
			"reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
		  event->subscribe.conn_handle,
		  event->subscribe.attr_handle,
		  event->subscribe.reason,
		  event->subscribe.prev_notify,
		  event->subscribe.cur_notify,
		  event->subscribe.prev_indicate,
		  event->subscribe.cur_indicate);
      return 0;

  case BLE_GAP_EVENT_MTU:
      MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
		  event->mtu.conn_handle,
		  event->mtu.channel_id,
		  event->mtu.value);
      return 0;

  case BLE_GAP_EVENT_REPEAT_PAIRING:
      /* We already have a bond with the peer, but it is attempting to
       * establish a new secure link.  This app sacrifices security for
       * convenience: just throw away the old bond and accept the new link.
       */

      /* Delete the old bond. */
      rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
      assert(rc == 0);
      ble_store_util_delete_peer(&desc.peer_id_addr);

      /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
       * continue with the pairing operation.
       */
      return BLE_GAP_REPEAT_PAIRING_RETRY;
  }

  return 0;
}

static int
lble_start_advertising() {
  uint8_t own_addr_type;
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  const char *name = gadget_name;
  int rc;

  /* Figure out address to use while advertising (no privacy for now) */
  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    return printf("error determining address type; rc=%d", rc);
  }

  /**
   *  Set the advertisement data included in our advertisements:
   *     o Flags (indicates advertisement type and other general info).
   *     o Advertising tx power.
   *     o Device name.
   *     o 16-bit service UUIDs (alert notifications).
   */

  memset(&fields, 0, sizeof fields);

  /* Advertise two flags:
   *     o Discoverability in forthcoming advertisement (general)
   *     o BLE-only (BR/EDR unsupported).
   */
  fields.flags = BLE_HS_ADV_F_DISC_GEN |
		 BLE_HS_ADV_F_BREDR_UNSUP;

  /* Indicate that the TX power level field should be included; have the
   * stack fill this value automatically.  This is done by assiging the
   * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
   */
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  size_t name_length = strlen(name);
  if (name_length < 16) {
    fields.name = (uint8_t *)name;
    fields.name_len = name_length;
    fields.name_is_complete = 1;
  } else {
    fields.name = (uint8_t *)name;
    fields.name_len = 16;
    fields.name_is_complete = 0;

    struct ble_hs_adv_fields scan_response_fields;
    memset(&scan_response_fields, 0, sizeof scan_response_fields);
    scan_response_fields.name = (uint8_t *)name;
    scan_response_fields.name_len = name_length;
    scan_response_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&scan_response_fields);
    if (rc) {
      return printf("gap_adv_rsp_set_fields failed: %d", rc);
    }
  }

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    return printf("error setting advertisement data; rc=%d", rc);
  }

  /* Begin advertising. */
  memset(&adv_params, 0, sizeof adv_params);
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
			 &adv_params, lble_gap_event, NULL);
  if (rc != 0) {
      return printf("error enabling advertisement; rc=%d", rc);
  }

  return 0;
}

static void
lble_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    lble_start_advertising();
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}


static int lble_init(lua_State *L) {
  if (!struct_pack_index) {
    lua_getglobal(L, "struct");
    lua_getfield(L, -1, "pack");
    struct_pack_index = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "unpack");
    struct_unpack_index = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);
  }
  // Passed the config table
  luaL_checktype(L, 1, LUA_TTABLE);

  printf("About to init_stack");
  lble_init_stack(L);

  lua_getfield(L, 1, "name");
  const char *name = strdup(luaL_checkstring(L, -1));

  free((void *) gadget_name);
  gadget_name = name;

  int rc;

  /* Initialize the NimBLE host configuration. */
  // ble_hs_cfg.reset_cb = bleprph_on_reset;
  ble_hs_cfg.sync_cb = lble_on_sync;
  ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  rc = gatt_svr_init(L);
  if (rc) {
    luaL_error(L, "Failed to gatt_svr_init: %d", rc);
  }

  return 0;
}

static int lble_shutdown(lua_State *L) {
  return 0;
}

LROT_BEGIN(lble, NULL, 0)
  LROT_FUNCENTRY( init, lble_init )
  LROT_FUNCENTRY( shutdown, lble_shutdown )
LROT_END(lble, NULL, 0)

NODEMCU_MODULE(BLE, "ble", lble, lble_sys_init);
#endif
