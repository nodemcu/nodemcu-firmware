/*
 * Copyright 2016 Dius Computing Pty Ltd. All rights reserved.
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

#include "platform.h"

#include <string.h>
#include <stdlib.h>
#include "esp_flash_partitions.h"
#include "esp_spi_flash.h"

static inline bool possible_idx (uint8_t idx)
{
  return ((idx +1) * sizeof (esp_partition_info_t)) < SPI_FLASH_SEC_SIZE;
}


bool platform_partition_info (uint8_t idx, platform_partition_t *info)
{
  if (!possible_idx (idx))
    return false;

  esp_partition_info_t pi;
  esp_err_t err = spi_flash_read (
    ESP_PARTITION_TABLE_OFFSET + idx * sizeof(pi), (uint32_t *)&pi, sizeof (pi));
  if (err != ESP_OK)
    return false;

  if (pi.magic != ESP_PARTITION_MAGIC)
    return false;

  memcpy (info->label, pi.label, sizeof (info->label));
  info->offs = pi.pos.offset;
  info->size = pi.pos.size;
  info->type = pi.type;
  info->subtype = pi.subtype;

  return true;
}


bool platform_partition_add (const platform_partition_t *info)
{
  esp_partition_info_t *part_table =
    (esp_partition_info_t *)malloc(SPI_FLASH_SEC_SIZE);
  if (!part_table)
    return false;
  esp_err_t err = spi_flash_read (
    ESP_PARTITION_TABLE_OFFSET, (uint32_t *)part_table, SPI_FLASH_SEC_SIZE);
  if (err != ESP_OK)
    goto out;

  uint8_t idx = 0;
  for (; possible_idx (idx); ++idx)
    if (part_table[idx].magic != ESP_PARTITION_MAGIC)
      break;

  if (possible_idx (idx))
  {
    esp_partition_info_t *slot = &part_table[idx];
    slot->magic = ESP_PARTITION_MAGIC;
    slot->type = info->type;
    slot->subtype = info->subtype;
    slot->pos.offset = info->offs;
    slot->pos.size = info->size;
    memcpy (slot->label, info->label, sizeof (slot->label));
    slot->flags = 0;
    err = spi_flash_erase_sector (
      ESP_PARTITION_TABLE_OFFSET / SPI_FLASH_SEC_SIZE);
    if (err == ESP_OK)
      err = spi_flash_write (
        ESP_PARTITION_TABLE_OFFSET, (uint32_t *)part_table, SPI_FLASH_SEC_SIZE);
  }

out:
  free (part_table);
  return err == ESP_OK;
}
