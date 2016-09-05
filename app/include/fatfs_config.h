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
// The first value is the physical drive and contains the GPIO pin for SS/CS of the SD card (default pin 8)
// The second value is the partition number.
#define NUM_LOGICAL_DRIVES 4
PARTITION VolToPart[NUM_LOGICAL_DRIVES] = {
  {8, 1},   /* Logical drive "0:" ==> SS pin 8, 1st partition */
  {8, 2},   /* Logical drive "1:" ==> SS pin 8, 2st partition */
  {8, 3},   /* Logical drive "2:" ==> SS pin 8, 3st partition */
  {8, 4}    /* Logical drive "3:" ==> SS pin 8, 4st partition */
};

#endif	/* __FATFS_CONFIG_H__ */
