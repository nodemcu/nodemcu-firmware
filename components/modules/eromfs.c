#include "module.h"
#include "lauxlib.h"

#include "esp_vfs.h"
#include <errno.h>
#include <dirent.h>

#include <stdint.h>
#include <string.h>

/**
 * The logical layout of the embedded volumes is
 * [ volume record ]
 * [ volume record ]
 * ...
 * [ index record in vol1 ]
 * [ index record in vol1 ]
 * ...
 * [ file contents in vol1 ]
 * [ file contents in vol1 ]
 * ...
 * [ index record in vol2 ]
 * [ index record in vol2 ]
 * ...
 * [ file contents in vol2 ]
 * [ file contents in vol2 ]
 *
 * Both the volume records and index records are variable length so as to not
 * waste space in their name fields. Finding the start of the index records
 * for a volume is by reading the offs(et) field in the volume record and
 * jumping that many bytes forward from the start of the eromfs.bin data.
 * Similarly, finding the file contents is by reading the index record's
 * offs(et) field and basing that off the start of the volume index.
 * Naturally, the start of the volume index is the same as the end of the
 * volume header, and the start of the file contents is the same as the
 * end of the volume index, and either of those can be worked out by
 * reading the offs(et) field in the first record.
 */

#pragma pack(push, 1)
typedef struct {
  uint8_t  rec_len;
  uint16_t offs; // index_offs
  char     name[];
} volume_record_t;

typedef struct {
  uint8_t  rec_len;
  uint32_t offs; // based off index_offs
  uint32_t len; // file_len
  char     name[];
} index_record_t;
#pragma pack(pop)


typedef struct {
  const index_record_t *meta;
  const char *data; // start of data
  off_t pos;
} file_descriptor_t;


typedef struct {
  DIR opaque;
  const index_record_t *index;
  const index_record_t *pos;
} eromfs_DIR_t;


extern const char eromfs_bin_start[] asm("_binary_eromfs_bin_start");

// Both the volume header and the file set indices end where the next
// type of data block commences (file set index, file contents).
#define end_of(type, start) ((const type *)(((char *)start) + start->offs))

#define eromfs_header_start ((const volume_record_t *)eromfs_bin_start)
#define eromfs_header_end   end_of(volume_record_t, eromfs_header_start)


/* The logic for finding a volume record by name is the same as finding a
 * file record by name, only the data structure type varies. Hence we
 * hide the casting and variable length record stepping behind a convenience
 * macro here.
 */
#define find_entry_by_name(out, xname, start_void_p, record_t) \
  do { \
    const record_t *entry_ = (const record_t *)(start_void_p); \
    const record_t *end_ = end_of(record_t, entry_); \
    for (; entry_ < end_; \
         entry_ = (const record_t *)(((char *)entry_) + entry_->rec_len)) \
    { \
      uint8_t name_len = entry_->rec_len - sizeof(record_t); \
      if (strncmp(xname, entry_->name, name_len) == 0) \
      { \
        out = entry_; \
        break; \
      } \
    } \
  } while(0);


static int mounted_volumes = LUA_NOREF;


static SemaphoreHandle_t fd_mutex;

static file_descriptor_t fds[CONFIG_NODEMCU_MAX_OPEN_FILES];


// --- VFS interface -----------------------------------------------------


#define get_index() const index_record_t *index = (const index_record_t *)ctx

static const index_record_t *path2entry(void *ctx, const char *path)
{
  while (*path == '/')
    ++path;

  get_index();
  const index_record_t *entry = NULL;
  find_entry_by_name(entry, path, index, index_record_t);
  return entry;
}


static int eromfs_fstat(void *ctx, int fd, struct stat *st)
{
  memset(st, 0, sizeof(struct stat));
  st->st_size = fds[fd].meta->len;
  st->st_blocks = (fds[fd].meta->len + 511)/512;
  return 0;
}


#ifdef CONFIG_VFS_SUPPORT_DIR
static int eromfs_stat(void *ctx, const char *path, struct stat *st)
{
  const index_record_t *entry = path2entry(ctx, path);
  if (!entry)
    return -ENOENT;
  memset(st, 0, sizeof(struct stat));
  st->st_size = entry->len;
  st->st_blocks = (entry->len + 511)/512;
  return 0;
}


static DIR *eromfs_opendir(void *ctx, const char *path)
{
  if (strcmp(path, "/") != 0)
    return NULL;

  get_index();
  eromfs_DIR_t *dir = calloc(1, sizeof(eromfs_DIR_t));
  dir->index = index;
  dir->pos = index;
  return (DIR *)dir;
}


static struct dirent *eromfs_readdir(void *ctx, DIR *pdir)
{
  UNUSED(ctx);
  eromfs_DIR_t *dir = (eromfs_DIR_t *)pdir;

  const index_record_t *end = end_of(index_record_t, dir->index);
  if (dir->pos >= end)
    return NULL;

  static struct dirent de = {
    .d_ino = 0,
    .d_type = DT_REG,
  };
  size_t max_len = sizeof(de.d_name);
  size_t len = dir->pos->rec_len - sizeof(index_record_t);
  if (len > max_len -1)
    len = max_len - 1;
  strncpy(de.d_name, dir->pos->name, len);
  de.d_name[len] = 0;
  dir->pos = (const index_record_t *)((char *)dir->pos + dir->pos->rec_len);
  return &de;
}


static int eromfs_closedir(void *ctx, DIR *dir)
{
  UNUSED(ctx);
  free(dir);
  return 0;
}
#endif


static int eromfs_open(void *ctx, const char *path, int flags, int mode)
{
  UNUSED(flags);
  UNUSED(mode);
  const index_record_t *entry = path2entry(ctx, path);
  if (!entry)
    return -ENOENT;

  xSemaphoreTake(fd_mutex, portMAX_DELAY);
  int fd = -ENFILE;
  // max open files is guaranteed to be small; linear search is fine
  for (unsigned i = 0; i < CONFIG_NODEMCU_MAX_OPEN_FILES; ++i)
  {
    if (fds[i].meta == NULL)
    {
      fds[i].meta = entry;
      fds[i].data = (const char *)ctx + entry->offs;
      fds[i].pos = 0;
      fd = (int)i;
    }
  }
  xSemaphoreGive(fd_mutex);

  return fd;
}


static ssize_t eromfs_read(void *ctx, int fd, void *dst, size_t size)
{
  UNUSED(ctx);
  size_t avail = fds[fd].meta->len - fds[fd].pos;
  if (size > avail)
    size = avail;
  const char *src = fds[fd].data + fds[fd].pos;
  memcpy(dst, src, size);
  fds[fd].pos += size;
  return size;
}


static off_t eromfs_lseek(void *ctx, int fd, off_t size, int mode)
{
  UNUSED(ctx);
  off_t pos = fds[fd].pos;
  switch(mode)
  {
    case SEEK_SET: pos = size; break;
    case SEEK_CUR: pos += size; break;
    case SEEK_END: pos = fds[fd].meta->len + size; break;
    default:
      return -EINVAL;
  }
  if (pos < 0 || pos > fds[fd].meta->len)
    return -EINVAL;
  fds[fd].pos = pos;
  return pos;
}


static int eromfs_close(void *ctx, int fd)
{
  UNUSED(ctx);
  xSemaphoreTake(fd_mutex, portMAX_DELAY);
  fds[fd].meta = NULL;
  fds[fd].data = NULL;
  fds[fd].pos = 0;
  xSemaphoreGive(fd_mutex);
  return 0;
}


// --- Lua interface -----------------------------------------------------

static int leromfs_list(lua_State *L)
{
  lua_newtable(L);
  int t = lua_gettop(L);
  // If this logic looks similar to the find_entry_by_name() macro, it's
  // because it is :)  Except we're capturing all the volume names, so no
  // easy reuse.
  const volume_record_t *vol = eromfs_header_start;
  const volume_record_t *end = eromfs_header_end;
  for (; vol < end; vol = (const volume_record_t *)((char *)vol + vol->rec_len))
  {
    uint8_t volume_name_len = vol->rec_len - sizeof(volume_record_t);
    lua_pushlstring(L, vol->name, volume_name_len);
    lua_rawseti(L, t, lua_objlen(L, t) + 1);
  }
  return 1;
}


static int leromfs_mount(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  const char *mountpt = luaL_checkstring(L, 2);
  lua_settop(L, 2);

  const volume_record_t *vol = NULL;
  find_entry_by_name(vol, name, eromfs_bin_start, volume_record_t);
  if (!vol)
    return luaL_error(L, "volume %s not found", name);

  const index_record_t *index_start =
    (const index_record_t *)(eromfs_bin_start + vol->offs);

  esp_vfs_t eromfs = {
    .flags = ESP_VFS_FLAG_CONTEXT_PTR,
    .open_p = eromfs_open,
    .fstat_p = eromfs_fstat,
    .read_p = eromfs_read,
    .lseek_p = eromfs_lseek,
    .close_p = eromfs_close,
#ifdef CONFIG_VFS_SUPPORT_DIR
    .stat_p = eromfs_stat,
    .opendir_p = eromfs_opendir,
    .readdir_p = eromfs_readdir,
    .closedir_p = eromfs_closedir,
#endif
  };
  esp_err_t err = esp_vfs_register(mountpt, &eromfs, (void *)index_start);
  if (err != ESP_OK)
    return luaL_error(L, "failed to mount eromfs; code %d", err);

  lua_rawgeti(L, LUA_REGISTRYINDEX, mounted_volumes);
  lua_pushvalue(L, 2);
  lua_pushvalue(L, 1);
  lua_rawset(L, -3); // mounted_volumes[mountpt] = name

  return 0;
}


static int leromfs_unmount(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  const char *mountpt = luaL_checkstring(L, 2);
  lua_settop(L, 2);

  lua_rawgeti(L, LUA_REGISTRYINDEX, mounted_volumes);
  lua_pushvalue(L, 2);
  lua_rawget(L, -2);
  if (lua_isstring(L, -1))
  {
    const char *mounted_name = lua_tostring(L, -1);
    if (strcmp(name, mounted_name) == 0)
    {
      esp_err_t err = esp_vfs_unregister(mountpt);
      if (err != ESP_OK)
        return luaL_error(L, "unmounting failed; code %d", err);
      lua_pop(L, 1);
      lua_pushvalue(L, 2);
      lua_pushnil(L);
      lua_rawset(L, -3); // mounted_volumes[mountpt] = nil
      return 0;
    }
    else
      return luaL_error(L,
        "can't umount %s from %s; volume %s is mounted there",
        name, mountpt, mounted_name);
  }
  else
    return 0; // already unmounted, not an error
}


static int leromfs_init(lua_State *L)
{
  fd_mutex = xSemaphoreCreateMutex();

  lua_newtable(L);
  mounted_volumes = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}


LROT_BEGIN(eromfs, NULL, 0)
  LROT_FUNCENTRY( list,    leromfs_list )
  LROT_FUNCENTRY( mount,   leromfs_mount )
  LROT_FUNCENTRY( unmount, leromfs_unmount )
LROT_END(eromfs, NULL, 0)

NODEMCU_MODULE(EROMFS, "eromfs", eromfs, leromfs_init);
