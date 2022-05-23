#include "lfs.h"

#include "esp_partition.h"
#include "nvs_flash.h"
#include "platform.h"

#if defined(CONFIG_NODEMCU_EMBEDDED_LFS_SIZE)
// Symbol provided by embedded_lfs component
extern const char lua_flash_store_reserved[0];
#endif


bool lfs_get_location(lfs_location_info_t *out)
{
#if defined(CONFIG_NODEMCU_EMBEDDED_LFS_SIZE)
  out->size      = CONFIG_NODEMCU_EMBEDDED_LFS_SIZE;
  out->addr_mem  = lua_flash_store_reserved;
  out->addr_phys = spi_flash_cache2phys(lua_flash_store_reserved);
  if (out->addr_phys == SPI_FLASH_CACHE2PHYS_FAIL) {
    NODE_ERR("spi_flash_cache2phys failed\n");
    return false;
  }
  else
    return true;
#else
  const esp_partition_t *part = esp_partition_find_first(
    PLATFORM_PARTITION_TYPE_NODEMCU,
    PLATFORM_PARTITION_SUBTYPE_NODEMCU_LFS,
    NULL);
  if (!part)
    return false; // Nothing to do if no LFS partition available

  out->size      = part->size; // in bytes
  out->addr_phys = part->address;
  out->addr_mem  = spi_flash_phys2cache(out->addr_phys, SPI_FLASH_MMAP_DATA);
  if (!out->addr_mem) { // not already mmap'd, have to do it ourselves
    spi_flash_mmap_handle_t ignored;
    esp_err_t err = spi_flash_mmap(
      out->addr_phys, out->size, SPI_FLASH_MMAP_DATA, &out->addr_mem, &ignored);
    if (err != ESP_OK) {
      NODE_ERR("Unable to access LFS partition - is it 64kB aligned as it needs to be?\n");
      return false;
    }
  }
  return true;
#endif
}

#define LFS_LOAD_NS "lfsload"
#define LFS_LOAD_FILE_KEY "filename"

bool lfs_get_load_filename(char *buf, size_t bufsiz)
{
  bool res = false;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(LFS_LOAD_NS, NVS_READONLY, &handle);
  if (err != ESP_OK)
    goto out;
  err = nvs_get_str(handle, LFS_LOAD_FILE_KEY, buf, &bufsiz);
  if (err != ESP_OK)
    goto close_out;
  res = true;
close_out:
  nvs_close(handle);
out:
  return res;
}


bool lfs_set_load_filename(const char *fname)
{
  bool res = false;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(LFS_LOAD_NS, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    goto out;
  err = nvs_set_str(handle, LFS_LOAD_FILE_KEY, fname);
  if (err != ESP_OK)
    goto close_out;
  res = true;
close_out:
  nvs_close(handle);
out:
  return res;
}


bool lfs_clear_load_filename(void)
{
  bool res = false;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(LFS_LOAD_NS, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    goto out;
  err = nvs_erase_key(handle, LFS_LOAD_FILE_KEY);
  if (err != ESP_OK)
    goto close_out;
  res = true;
close_out:
  nvs_close(handle);
out:
  return res;
}
