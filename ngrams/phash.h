#ifndef PHASH_H
#define PHASH_H

#include <stdint.h>

typedef struct entry {
  void* data;
  uint32_t next;
} entry_t;

typedef struct phash {
  void* data;
  int count;
  int entries_alloc;
  int bucket_count;
  entry_t *entries;
  uint32_t *buckets;
  char *filename;
  void* ptr;
  int alloc;
  int error;
} phash_t;

phash_t* ph_init(char *filename);
void* ph_get(phash_t* h, char* key, int *return_length);
void ph_set(phash_t* h, char* key, void *data, int length);
void ph_save(phash_t* h);
void ph_rm(phash_t* h, char* key);
void ph_close(phash_t* h);

#endif /* PHASH_H */
