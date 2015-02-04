#ifndef FSLM_H
#define FSLM_H

#define MAX_TABLE 5

typedef struct block {
  int table_offset;
  int context_offset;
  void *data;
} block_t;

typedef struct table {
  int block_count;
  int *word_start_offset;
  int *word_end_offset;
  block_t *blocks;
} table_t;

typedef struct db {
  int table_count;
  table_t tables[MAX_TABLE];
  int block_size;
  int base_bits;
} db_t;


db_t* db_init(void* data);
void db_free(db_t *db);
int lookup_table(db_t *db, int table_index, int wid, int target_context_offset, int *return_offset);
int search(db_t *db, int wids[], int count);

#endif /* FSLM_H */

