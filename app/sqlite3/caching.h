#ifndef _CACHING_H
#define _CACHING_H
#define CACHEBLOCKSZ 64

typedef struct st_linkedlist {
        uint16_t blockid;
        struct st_linkedlist *next;
        uint8_t data[CACHEBLOCKSZ];
} linkedlist_t, *pLinkedList_t;

typedef struct st_filecache {
        uint32_t size;
        linkedlist_t *list;
} filecache_t, *pFileCache_t;

uint32_t filecache_push (pFileCache_t, uint32_t, uint32_t, const uint8_t *);
uint32_t filecache_pull (pFileCache_t, uint32_t, uint32_t, uint8_t *);
void filecache_free (pFileCache_t);
#endif
