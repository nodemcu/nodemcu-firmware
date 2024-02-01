/*
 * Copyright 2019 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */

#include "module.h"
#include "lauxlib.h"

#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"

static esp_ota_handle_t ota_handle;
static const esp_partition_t *next;

// ----------otaupgrade Lua API ------------------------------------------


// Lua: otaupgrade.commence() -- wipes an inactive slot and enables .write()
static int lotaupgrade_commence (lua_State* L)
{
  next = esp_ota_get_next_update_partition (NULL);
  if (!next)
    return luaL_error (L, "no OTA partition available");

  esp_err_t err = esp_ota_begin (next, OTA_SIZE_UNKNOWN, &ota_handle);
  const char *msg = NULL;
  switch (err) {
    case ESP_OK: break;
    case ESP_ERR_NO_MEM: msg = "out of memory"; break;
    case ESP_ERR_OTA_PARTITION_CONFLICT: // I don't think we can get this?
      msg = "can't overrite running firmware"; break;
    case ESP_ERR_OTA_SELECT_INFO_INVALID:
      msg = "ota data partition invalid"; break;
    case ESP_ERR_OTA_ROLLBACK_INVALID_STATE:
      msg = "can't upgrade from unconfirmed firmware"; break;
    default: msg = "ota error"; break;
  }
  if (msg)
    return luaL_error (L, msg);
  else
    return 0;
}

// Lua: otaupgrade.write(data) -- writes the data block to flash
static int lotaupgrade_write (lua_State *L)
{
  const char *bytes = luaL_checkstring (L, 1);
  size_t len = lua_objlen (L, 1);

  esp_err_t err = esp_ota_write (ota_handle, bytes, len);
  const char *msg = NULL;
  switch (err) {
    case ESP_OK: break;
    case ESP_ERR_INVALID_ARG:
      msg = "write not possible, use otaupgrade.commence() first"; break;
    case ESP_ERR_OTA_VALIDATE_FAILED: msg = "not a valid ota image"; break;
    default: msg = "ota error"; break;
  }
  if (msg)
    return luaL_error (L, msg);
  else
    return 0;
}


// Lua: otaupgrade.complete(optional_reboot)
static int lotaupgrade_complete (lua_State *L)
{
  if (!next)
    return luaL_error (L, "no upgrade in progress");

  esp_err_t err = esp_ota_end(ota_handle);
  const char *msg = NULL;
  switch (err) {
    case ESP_OK: break;
    case ESP_ERR_INVALID_ARG: msg = "empty firmware image"; break;
    case ESP_ERR_OTA_VALIDATE_FAILED: msg = "validation failed"; break;
    default: msg = "ota error"; break;
  }
  if (msg)
    return luaL_error (L, msg);

  err = esp_ota_set_boot_partition (next);
  next = NULL;

  if (luaL_optint (L, 1, 0))
    esp_restart ();

  return 0;
}

// Lua: otaupgrade.accept()
static int lotaupgrade_accept (lua_State *L)
{
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err != ESP_OK) // only ESP_OK defined as expected return value
    return luaL_error(L, "firmware accept failed");
  else
    return 0;
}

// Lua: otaupgrade.rollback()
static int lotaupgrade_rollback (lua_State *L)
{
  esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
  const char *msg = NULL;
  switch (err) {
    case ESP_OK: break;
    case ESP_ERR_OTA_ROLLBACK_FAILED:
      msg = "no other firmware to roll back to"; break;
    default:
      msg = "ota error"; break;
  }
  if (msg)
    return luaL_error(L, msg);
  else
    return 0; // actually, we never get here as on success the chip reboots
}


/* Lua: t = otaupgrade.info ()
 * -- running_partition, nextboot_partition, {
 *      .X = { name, version, secure_version, date, time, idf_version, state },
 *      .Y = { name, version, secure_version, date, time, idf_version, state }
 *    }
 */
static int lotaupgrade_info (lua_State *L)
{
  const esp_partition_t *running = esp_ota_get_running_partition ();
  if (running)
    lua_pushstring (L, running->label);
  else
    lua_pushnil (L);

  const esp_partition_t *boot = esp_ota_get_boot_partition ();
  if (boot)
    lua_pushstring (L, boot->label);
  else
    lua_pushnil (L);

  lua_createtable (L, 0, 2);
  esp_partition_iterator_t iter = esp_partition_find (
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (iter) {
    const esp_partition_t *part = esp_partition_get (iter);
    if (part->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
        part->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
    {
      lua_pushstring (L, part->label);
      lua_createtable (L, 0, 6);

      esp_ota_img_states_t state;
      esp_err_t err = esp_ota_get_state_partition (part, &state);

      if (err == ESP_OK)
      {
        lua_pushliteral (L, "state");
        const char *msg = "";
        switch (state) {
          case ESP_OTA_IMG_NEW: msg = "new"; break;
          case ESP_OTA_IMG_PENDING_VERIFY: msg = "testing"; break;
          case ESP_OTA_IMG_VALID: msg = "valid"; break;
          case ESP_OTA_IMG_INVALID: msg = "invalid"; break;
          case ESP_OTA_IMG_ABORTED: msg = "aborted"; break;
          case ESP_OTA_IMG_UNDEFINED: // fall-through
          default: msg = "undefined"; break;
        }
        lua_pushstring (L, msg);
        lua_settable (L, -3);
      }
      else
        goto next; // just add an empty table for this slot

      esp_app_desc_t desc;
      err = esp_ota_get_partition_description(part, &desc);

      if (err == ESP_OK)
      {
        lua_pushliteral (L, "name");
        lua_pushstring (L, desc.project_name);
        lua_settable (L, -3);

        lua_pushliteral (L, "version");
        lua_pushstring (L, desc.version);
        lua_settable (L, -3);

        lua_pushliteral (L, "secure_version");
        lua_pushinteger (L, desc.secure_version);
        lua_settable (L, -3);

        lua_pushliteral (L, "date");
        lua_pushstring (L, desc.date);
        lua_settable (L, -3);

        lua_pushliteral (L, "time");
        lua_pushstring (L, desc.time);
        lua_settable (L, -3);

        lua_pushliteral (L, "idf_version");
        lua_pushstring (L, desc.idf_ver);
        lua_settable (L, -3);
      }

next:
      lua_settable (L, -3); // info table into return arg #3 table
    }
    iter = esp_partition_next (iter);
  }
  return 3;
}


LROT_BEGIN(otaupgrade, NULL, 0)
  LROT_FUNCENTRY( commence,   lotaupgrade_commence )
  LROT_FUNCENTRY( write,      lotaupgrade_write    )
  LROT_FUNCENTRY( complete,   lotaupgrade_complete )
  LROT_FUNCENTRY( accept,     lotaupgrade_accept   )
  LROT_FUNCENTRY( rollback,   lotaupgrade_rollback )
  LROT_FUNCENTRY( info,       lotaupgrade_info     )
LROT_END(otaupgrade, NULL, 0)

NODEMCU_MODULE(OTAUPGRADE, "otaupgrade", otaupgrade, NULL);
