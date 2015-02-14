#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "phash.h"
#include "pack.h"

#define INCREASE 1.05
#define MAX_KEY_LEN 128
#define ALLOC_ENTRIES 5000
#define ALLOC_DATA 50000

#define CANARY 0x29a35f85


static unsigned long hash_str(unsigned char *str) {
  // djb2 @ https://stackoverflow.com/questions/7666509/hash-function-for-string
  unsigned long hash = 5381;
  int c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

static void free_data(phash_t* h) {
  if (h->data) { free(h->data); }
  if (h->entries) { free(h->entries); }
  if (h->buckets) { free(h->buckets); }
  h->data = NULL;
  h->entries = NULL;
  h->buckets = NULL;
  memset(h, 0, sizeof(h));
}

static int load(phash_t* h) {
  struct stat st;
  int len, fd, bytes_read;
  void *ptr;
  int i;
  int count;
  int load_file = 0;
  fd = -1;

  free_data(h);

  if (! stat(h->filename, &st)) {
    len = st.st_size;
    if (len > 0) {
      fd = open(h->filename, O_RDONLY);
      if (fd == -1) { goto fail; }
      load_file = 1;
    }
  } else {
    len = 0;
  }

  h->alloc = (INCREASE * len) + ALLOC_DATA; // add 10% for increase
  h->data = malloc(h->alloc);
  if (! h->data) { goto fail; }

  h->count = 1;
  if (load_file) {
    bytes_read = read(fd, h->data, len);
    if (len != bytes_read) { goto fail; }
    count = get_uint32(h->data);
    h->entries_alloc = INCREASE * count + ALLOC_ENTRIES;
  } else {
    h->entries_alloc = ALLOC_ENTRIES;
    count = 0; // dump compiler warning
  }

  h->entries = (entry_t*) malloc(sizeof(entry_t) * h->entries_alloc);
  if (! h->entries) { goto fail; }
  memset(h->entries, 0, sizeof(entry_t) * h->entries_alloc);

  h->bucket_count = h->entries_alloc;
  h->buckets = (uint32_t*) malloc(sizeof(uint32_t) * h->bucket_count);
  if (! h->buckets) { goto fail; }
  memset(h->buckets, 0, sizeof(uint32_t) * h->bucket_count);

  ptr = h->data + 4;
  if (load_file) {
    for(i = 0; i < count; i ++) {
      int index, hash_value;
      int kl, vl;
      entry_t* entry;

      kl = strlen(ptr);
      if (kl > MAX_KEY_LEN) { goto fail; }
      if (ptr > h->data + len) { goto fail; }
      vl = get_char(ptr + 1 + kl);
      if (vl == 0) { goto fail; }

      hash_value = hash_str((unsigned char *) ptr) % h->bucket_count;
      index = (h->count ++);
      entry = &(h->entries[index]);
      entry->data = ptr;
      entry->next = h->buckets[hash_value];
      h->buckets[hash_value] = index;

      ptr += 2 + kl + vl;
    }
    if (get_uint32(ptr) != CANARY) { goto fail; } // integrity
    close(fd);
  }

  h->ptr = ptr;

  return 0;

 fail:
  if (fd != -1) { close(fd); }
  h->error = 1;
  free_data(h);
  return 1;
}

void ph_close(phash_t* h) {
  free_data(h);
  if (h->filename) { free(h->filename); h->filename = NULL; }
  free(h);
}

phash_t* ph_init(char* filename) {
  phash_t* h;

  h = (phash_t*) malloc(sizeof(phash_t));
  if (! h) { return NULL; }
  memset(h, 0, sizeof(phash_t));

  h->filename = strdup(filename);
  if (! h->filename) { free(h); return NULL; }

  if (load(h)) { ph_close(h); return NULL; }

  return h;
}

void* ph_get(phash_t* h, char* key, int *return_length) {
  int hash_value = hash_str((unsigned char *) key) % h->bucket_count;
  int index = h->buckets[hash_value];
  while (index > 0) {
    entry_t* entry = &(h->entries[index]);
    void* ptr = entry->data;
    if (ptr) {
      if (! strcmp(ptr, key)) {
	int l;
	ptr += strlen(entry->data) + 1;
	l = *((unsigned char*) ptr);
	ptr ++;
	if (return_length) { *return_length = l; }
	return ptr;
      }
    }
    index = entry->next;
  }

  return NULL;
}

void ph_set(phash_t* h, char* key, void *data, int length) {
  int hash_value, index, lk;
  void* ptr;
  entry_t* entry;

  if (h->count > h->entries_alloc - 5 ||
      h->ptr > h->data + h->alloc - 1000) {
    ph_save(h);
    load(h);
  }

  hash_value = hash_str((unsigned char *) key) % h->bucket_count;
  index = h->buckets[hash_value];

  if (length > 255) { length = 255; }

  lk = strlen(key);
  if (length > 0) {
    ptr = h->ptr;
    strcpy(h->ptr, key);
    h->ptr += lk + 1;
    *((unsigned char*) h->ptr) = length;
    h->ptr += 1;
    memcpy(h->ptr, data, length);
    h->ptr += length;
  } else {
    ptr = NULL;
  }

  while (index > 0) {
    entry_t* entry = &(h->entries[index]);
    void* pe = entry->data;
    if (pe) {
      if (! strcmp(pe, key)) {
	entry->data = ptr;
	return;
      }
    }
    index = entry->next;
  }

  if (! length) { return; }

  index = (h->count ++);
  entry = &(h->entries[index]);
  entry->data = ptr;
  entry->next = h->buckets[hash_value];
  h->buckets[hash_value] = index;
}

#define BUF 10240
void ph_save(phash_t* h) {
  char tmpname[256];
  char tmp[4];
  int fd, count, i;
  char buf[BUF];
  char *pbuf;

  strncpy(tmpname, h->filename, sizeof(tmpname) - 6);
  strcat(tmpname, ".tmp");

  fd = open(tmpname, O_WRONLY | O_CREAT, 0644);
  if (fd == -1) { goto fail; }

  if (write(fd, "XXXX", 4) != 4) { goto fail; }

  pbuf = buf;
  count = 0;
  for(i = 0; i < h->bucket_count; i++) {
    int index = h->buckets[i];
    while (index > 0) {
      entry_t* entry = &(h->entries[index]);
      void* ptr = entry->data;
      if (ptr) {
	char *key = ptr;
	unsigned char lk, lv;

	lk = strlen(key);
	ptr += lk + 1;
	lv = *((unsigned char*) ptr);
	ptr ++;
	if (lk > 0) {
	  strcpy(pbuf, key); pbuf += lk + 1;
	  *pbuf = lv; pbuf += 1;
	  memcpy(pbuf, ptr, lv); pbuf += lv;
	  if (pbuf - buf >= BUF - 512) {
	    int l = pbuf - buf;
	    if (write(fd, buf, l) != l) { goto fail; }
	    pbuf = buf;
	  }
	  count ++;
	}
      }
      index = entry->next;
    }
  }
  if (pbuf > buf) {
    int l = pbuf - buf;
    if (write(fd, buf, l) != l) { goto fail; }
  }
  set_uint32(tmp, CANARY);
  if (write(fd, tmp, 4) != 4) { goto fail; }
  if (lseek(fd, 0, SEEK_SET) == -1) { goto fail; }

  set_uint32(tmp, count);
  if (write(fd, tmp, 4) != 4) { goto fail; }

  close(fd);

  rename(tmpname, h->filename);

  return;

 fail:
  h->error = 2;
  return;
}

void ph_rm(phash_t* h, char* key) {
  ph_set(h, key, NULL, 0);
}
