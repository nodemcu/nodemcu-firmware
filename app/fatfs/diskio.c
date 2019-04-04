/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* FatFs lower layer API */
#include "sdcard.h"

static DSTATUS m_status = STA_NOINIT;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
  return m_status;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
  int result;

  if (platform_sdcard_init( 1, pdrv )) {
    m_status &= ~STA_NOINIT;
  }

  return m_status;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
  if (count == 1) {
    if (! platform_sdcard_read_block( pdrv, sector, buff )) {
      return RES_ERROR;
    }
  } else {
    if (! platform_sdcard_read_blocks( pdrv, sector, count, buff )) {
      return RES_ERROR;
    }
  }

  return RES_OK;
}


/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
  if (count == 1) {
    if (! platform_sdcard_write_block( pdrv, sector, buff )) {
      return RES_ERROR;
    }
  } else {
    if (! platform_sdcard_write_blocks( pdrv, sector, count, buff )) {
      return RES_ERROR;
    }
  }

  return RES_OK;
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
  switch (cmd) {
  case CTRL_TRIM:    /* no-op */
  case CTRL_SYNC:    /* no-op */
    return RES_OK;

  default:           /* anything else throws parameter error */
    return RES_PARERR;
  }
}
