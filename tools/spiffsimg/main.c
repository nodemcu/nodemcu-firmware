/*
 * Copyright 2015 Dius Computing Pty Ltd. All rights reserved.
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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include <errno.h>
#include "spiffs.h"
#define NO_CPU_ESP8266_INCLUDE
#include "../platform/cpu_esp8266.h"

static spiffs fs;
static uint8_t *flash;

static int retcode = 0;

#define LOG_PAGE_SIZE 256

// If the caclulated size is this or less, then use smaller blocks
// (1 page) rather than the normal 2 pages. This gives a little bit
// more flexibility for the file system, at the expense of a bit of
// efficiency
#define SMALL_FILESYSTEM	(128 * 1024)

static int delete_on_die = 0;
static const char *delete_list[10];
static int delete_list_index = 0;

static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];

static s32_t flash_read (u32_t addr, u32_t size, u8_t *dst) {
  memcpy (dst, flash + addr, size);
  return SPIFFS_OK;
}

static s32_t flash_write (u32_t addr, u32_t size, u8_t *src) {
  memcpy (flash + addr, src, size);
  return SPIFFS_OK;
}

static s32_t flash_erase (u32_t addr, u32_t size) {
  memset (flash + addr, 0xff, size);
  return SPIFFS_OK;
}


static void die (const char *what)
{
  if (errno == 0) {
    fprintf(stderr, "%s: fatal error\n", what);
  } else {
    perror (what);
  }
  if (delete_on_die) {
    const char **p = delete_list;
    while (*p) {
      unlink(*p);
      p++;
    }
  }
  exit (1);
}


static void list (void)
{
  spiffs_DIR dir;
  if (!SPIFFS_opendir (&fs, "/", &dir))
    die ("spiffs_opendir");
  struct spiffs_dirent de;
  while (SPIFFS_readdir (&dir, &de))
  {
    static const char types[] = "?fdhs"; // file, dir, hardlink, softlink
    char name[sizeof(de.name)+1] = { 0 };
    memcpy (name, de.name, sizeof(de.name));
    printf("%c %6u %s\n", types[de.type], de.size, name);
  }
  SPIFFS_closedir (&dir);
}


static void cat (char *fname)
{
  spiffs_file fh = SPIFFS_open (&fs, fname, SPIFFS_RDONLY, 0);
  char buff[512];
  s32_t n;
  while ((n = SPIFFS_read (&fs, fh, buff, sizeof (buff))) > 0)
    write (STDOUT_FILENO, buff, n);
  SPIFFS_close (&fs, fh);
}


static void import (char *src, char *dst)
{
  int fd = open (src, O_RDONLY);
  if (fd < 0)
    die (src);

  spiffs_file fh = SPIFFS_open (&fs, dst, SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_WRONLY, 0);
  if (fh < 0)
    die ("spiffs_open");

  char buff[16384];
  s32_t n;
  while ((n = read (fd, buff, sizeof (buff))) > 0)
    if (SPIFFS_write (&fs, fh, buff, n) < 0)
      die ("spiffs_write");

  if (SPIFFS_close (&fs, fh) < 0) 
    die("spiffs_close");
  close (fd);
}


static void export (char *src, char *dst)
{
  spiffs_file fh = SPIFFS_open (&fs, src, SPIFFS_RDONLY, 0);
  if (fh < 0)
    die ("spiffs_open");

  int fd = open (dst, O_CREAT | O_TRUNC | O_WRONLY, 0664);
  if (fd < 0)
    die (dst);

  char buff[512];
  s32_t n;
  while ((n = SPIFFS_read (&fs, fh, buff, sizeof (buff))) > 0)
    if (write (fd, buff, n) < 0)
      die ("write");

  SPIFFS_close (&fs, fh);
  close (fd);
}


char *trim (char *in)
{
  if (!in)
    return "";

  char *out = 0;
  while (*in)
  {
    if (!out && !isspace (*in))
      out = in;
    ++in;
  }
  if (!out)
    return "";
  while (--in > out && isspace (*in))
    ;
  in[1] = 0;
  return out;
}


void syntax (void)
{
  fprintf (stderr,
    "Syntax: spiffsimg -f <filename> [-d] [-o <locationfilename>] [-c size] [-S flashsize] [-U usedsize] [-l | -i | -r <scriptname> ]\n\n"
  );
  exit (1);
}

static size_t getsize(const char *s) 
{
  char *end = 0;
  size_t val = strtoul(s, &end, 0);

  if (end) {
    int factor;
    if (*end == 'k' || *end == 'K') {
      factor = 1 << 10;
      end++;
    } else if (*end == 'm' || *end == 'M') {
      factor = 1 << 20;
      end++;
    } else {
      factor = 1;
    }
    // Capital B is bytes
    if (*end == 'B') {
      factor = factor << 3;
    }
    // we want bytes
    val = (val * factor) / 8;
  }

  return val;
}

int main (int argc, char *argv[])
{
  if (argc == 1)
    syntax ();

  int opt;
  const char *fname = 0;
  bool create = false;
  enum { CMD_NONE, CMD_LIST, CMD_INTERACTIVE, CMD_SCRIPT } command = CMD_NONE;
  int sz = 0;
  const char *script_name = 0;
  const char *resolved = 0;
  int flashsize = 0;
  int used = 0;
  while ((opt = getopt (argc, argv, "do:f:c:lir:S:U:")) != -1)
  {
    switch (opt)
    {
      case 'f': fname = optarg; break;
      case 'o': resolved = optarg; break;
      case 'c': create = true; sz = strtol(optarg, 0, 0); break;
      case 'S': create = true; flashsize = getsize(optarg); break;
      case 'U': create = true; used = strtol(optarg, 0, 0); break;
      case 'd': delete_on_die = 1; break;
      case 'l': command = CMD_LIST; break;
      case 'i': command = CMD_INTERACTIVE; break;
      case 'r': command = CMD_SCRIPT; script_name = optarg; break;
      default: die ("unknown option");
    }
  }

  if (!fname) {
    die("Need a filename");
  }

  int fd;

  if (create)
  {
    if (!sz) {
      if (!flashsize || !used) {
	die("Missing flashSize or Used");
      }
      sz = flashsize - used - SYS_PARAM_SEC_NUM * 4096;   // This leaves space for the parameter area
      if (sz < 0x4000) {
	die("Not enough space");
      }
      if (sz > SMALL_FILESYSTEM) {
	used = (used + 0xffff) & ~0xffff;
      } else {
	used = (used + 0x1fff) & ~0x1fff;
      }
      sz = flashsize - used - SYS_PARAM_SEC_NUM * 4096;
    }
    sz &= ~(0x1fff);

    if (used == 0) {
      if (strchr(fname, '%')) {
	die("Unable to calculate where to put the flash image, and % present in filename");
      }
    }

    char fnamebuff[1024];

    sprintf(fnamebuff, fname, used);

    delete_list[delete_list_index++] = strdup(fnamebuff);;
    fd = open (fnamebuff, (create ? (O_CREAT | O_TRUNC) : 0) | O_RDWR, 0664);
    if (fd == -1)
      die ("open");

    if (resolved) {
      delete_list[delete_list_index++] = resolved;
      FILE *f = fopen(resolved, "w");
      fprintf(f, "0x%x", used);
      fclose(f);
    }

    if (lseek (fd, sz -1, SEEK_SET) == -1)
      die ("lseek");
    if (write (fd, "", 1) != 1)
      die ("write");
  }
  else {
    fd = open (fname, O_RDWR, 0664);
    if (fd == -1)
      die ("open");

    if (!sz)
    {
      off_t offs = lseek (fd, 0, SEEK_END);
      if (offs == -1)
	die ("lseek");
      sz = offs;
    }
  }

  if (sz & (0x1000 -1))
    die ("file size not multiple of erase block size");

  flash = mmap (0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (!flash)
    die ("mmap");

  if (create)
    memset (flash, 0xff, sz);

  spiffs_config cfg;
  cfg.phys_size = sz;
  cfg.phys_addr = 0;
  cfg.phys_erase_block = 0x1000;
  cfg.log_block_size = 0x1000 * (sz > SMALL_FILESYSTEM ? 2 : 1);
  cfg.log_page_size = LOG_PAGE_SIZE;
  cfg.hal_read_f = flash_read;
  cfg.hal_write_f = flash_write;
  cfg.hal_erase_f = flash_erase;
  if (cfg.phys_size < 4 * cfg.log_block_size) {
    die("disk not large enough for four blocks");
  }

  if (SPIFFS_mount (&fs, &cfg,
      spiffs_work_buf,
      spiffs_fds,
      sizeof(spiffs_fds),
      malloc(65536), 65536, 0) != 0) {
    if (create) {
      if (SPIFFS_format(&fs) != 0) {
        die("spiffs_format");
      }
      if (SPIFFS_mount (&fs, &cfg,
          spiffs_work_buf,
          spiffs_fds,
          sizeof(spiffs_fds),
          malloc(65536), 65536, 0) != 0) {
        die ("spiffs_mount");
      }
      if (command == CMD_INTERACTIVE) {
	printf("Created filesystem -- size 0x%x, block_size=%d\n", cfg.phys_size, cfg.log_block_size);
      }
    } else {
      die ("spiffs_mount");
    }
  }

  if (command == CMD_NONE)
    ; // maybe just wanted to create an empty image?
  else if (command == CMD_LIST)
    list ();
  else
  {
    FILE *in = (command == CMD_INTERACTIVE) ? stdin : fopen (script_name, "r");
    if (!in)
      die ("fopen");
    char buff[128] = { 0 };
    if (in == stdin)
      printf("> ");
    while (fgets (buff, sizeof (buff) -1, in))
    {
      char *line = trim (buff);
      if (!line[0] || line[0] == '#')
        continue;
      if (strcmp (line, "ls") == 0)
        list ();
      else if (strncmp (line, "import ", 7) == 0)
      {
        char *src = 0, *dst = 0;
        if (sscanf (line +7, " %ms %ms", &src, &dst) != 2)
        {
          fprintf (stderr, "SYNTAX ERROR: %s\n", line);
          retcode = 1;
        }
        else
          import (src, dst);
        free (src);
        free (dst);
      }
      else if (strncmp (line, "export ", 7) == 0)
      {
        char *src = 0, *dst = 0;
        if (sscanf (line + 7, " %ms %ms", &src, &dst) != 2)
        {
          fprintf (stderr, "SYNTAX ERROR: %s\n", line);
          retcode = 1;
        }
        else
          export (src, dst);
        free (src);
        free (dst);
      }
      else if (strncmp (line, "rm ", 3) == 0)
      {
        if (SPIFFS_remove (&fs, trim (line + 3)) < 0)
        {
          fprintf (stderr, "FAILED: %s\n", line);
          retcode = 1;
        }
      }
      else if (strncmp (line, "cat ", 4) == 0)
        cat (trim (line + 4));
      else if (strncmp (line, "info", 4) == 0)
      {
        u32_t total, used;
        if (SPIFFS_info (&fs, &total, &used) < 0)
        {
          fprintf (stderr, "FAILED: %s\n", line);
          retcode = 1;
        }
        else
          printf ("Total: %u, Used: %u\n", total, used);
      }
      else
      {
        printf ("SYNTAX ERROR: %s\n", line);
        retcode = 1;
      }

      if (in == stdin)
        printf ("> ");
    }
    if (in == stdin)
      printf ("\n");
  }

  SPIFFS_unmount (&fs);
  munmap (flash, sz);
  close (fd);
  return retcode;
}
