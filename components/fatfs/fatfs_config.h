#ifndef __FATFS_CONFIG_H__
#define __FATFS_CONFIG_H__


// don't redefine the PARTITION type
#ifndef FF_DEFINED
typedef struct {
	BYTE pd;	/* Physical drive number */
	BYTE pt;	/* Partition: 0:Auto detect, 1-4:Forced partition) */
} PARTITION;
#endif

// Table to map physical drive & partition to a logical volume.
// The first value is the physical drive which relates to the SDMMC slot number
// The second value is the partition number.
PARTITION VolToPart[FF_VOLUMES] = {
  {0, 1},   /* Logical drive "0:" ==> slot 0, 1st partition */
  {0, 2},   /* Logical drive "1:" ==> slot 0, 2st partition */
  {0, 3},   /* Logical drive "2:" ==> slot 0, 3st partition */
  {0, 4}    /* Logical drive "3:" ==> slot 0, 4st partition */
};

#endif	/* __FATFS_CONFIG_H__ */
