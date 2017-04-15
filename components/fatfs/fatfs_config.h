#ifndef __FATFS_CONFIG_H__
#define __FATFS_CONFIG_H__


// don't redefine the PARTITION type
#ifndef _FATFS
typedef struct {
	BYTE pd;	/* Physical drive number */
	BYTE pt;	/* Partition: 0:Auto detect, 1-4:Forced partition) */
} PARTITION;
#endif

// Table to map physical drive & partition to a logical volume.
// The first value is the physical drive which relates to the SDMMC slot number
// The second value is the partition number.
#define NUM_LOGICAL_DRIVES 4
PARTITION VolToPart[NUM_LOGICAL_DRIVES] = {
  {1, 1},   /* Logical drive "0:" ==> slot 1, 1st partition */
  {1, 2},   /* Logical drive "1:" ==> slot 1, 2st partition */
  {1, 3},   /* Logical drive "2:" ==> slot 1, 3st partition */
  {1, 4}    /* Logical drive "3:" ==> slot 1, 4st partition */
};

#endif	/* __FATFS_CONFIG_H__ */
