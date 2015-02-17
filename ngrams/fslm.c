/*
This file is part of OKBoard project

Copyright (c) 2015, Eric Berenguier <eb@cpbm.eu>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

==============================================================================

Fast & compact N-grams storage

This is an implementation of this (excellent) paper:

http://nlp.cs.berkeley.edu/pubs/Pauls-Klein_2011_LM_paper.pdf
Faster and Smaller N-Gram Language Models
Adam Pauls Dan Klein
Computer Science Division
University of California, Berkeley
{adpauls,klein}@cs.berkeley.edu

This file only contains the decoder from fslm_ngram.py
(re-implemented in C language)
*/

#include <stdint.h>
#include <stdlib.h>
#include "fslm.h"
#include "pack.h"

int read_bits(int count, void** ptr, int* pos) {
  int value = 0;
  int n;

  while(1) {
    if (*pos == 8) {
      *pos = 0;
      (*ptr) ++;
    }

    n = 8 - *pos;
    if (count < n) { n = count; }

    value <<= n;
    value += ((*((unsigned char*) *ptr)) >> (8 - n - *pos)) & ((1 << n) - 1);
    *pos += n;
    count -= n;

    if (! count) { return value; }
  }
}

int get_variable_length(int base_bits, void **ptr, int* pos) {
  int b = base_bits;
  while (! read_bits(1, ptr, pos)) {
    b += base_bits;
  }
  return read_bits(b, ptr, pos);
}

int parse_table(table_t* table, void* ptr, int block_size) {
  int max_wid, blocks_count;
  int i;

  max_wid = get_uint32(ptr);
  /* not used: blocks_bytes = get_uint32(ptr + 4); */
  blocks_count = get_uint32(ptr + 8);

  ptr += 12;

  table->word_start_offset = (int*) malloc(sizeof(int) * (max_wid + 1));
  table->word_end_offset = (int*) malloc(sizeof(int) * (max_wid + 1));

  if (! table->word_start_offset || ! table->word_end_offset) { return 1; }

  for(i = 0; i < max_wid; i ++) {
    table->word_start_offset[i] = get_int32(ptr);
    table->word_end_offset[i] = get_int32(ptr + 4);
    ptr += 8;
  }

  table->blocks = (block_t *) malloc(sizeof(block_t) * (blocks_count + 1));
  if (! table->blocks) { return 1; };

  for(i = 0; i < blocks_count; i ++) {
    table->blocks[i].table_offset = get_uint32(ptr);
    table->blocks[i].context_offset = get_uint32(ptr + 4);
    table->blocks[i].data = ptr + 8;
    ptr += block_size;
  }

  table->block_count = blocks_count;
  table->max_wid = max_wid;

  return 0;
}

db_t* db_init(void* data) {
  db_t *db;
  void *ptr;
  int i;

  db = (db_t*) malloc(sizeof(db_t));
  if (! db) return NULL;

  ptr = (void*) data;

  db->block_size = get_uint16(ptr);
  db->base_bits = get_char(ptr + 2);
  db->table_count = get_char(ptr + 3);

  if (db->table_count > MAX_TABLE) { return NULL; }

  ptr += 4;
  for(i = 0; i < db->table_count; i++) {
    int table_len = get_uint32(ptr);
    ptr += 4;
    if (parse_table(&(db->tables[i]), ptr, db->block_size)) { return NULL; }
    ptr += table_len;
  }

  return db;
}

void db_free(db_t *db) {
  int i;
  for (i = 0; i < db->table_count; i ++) {
    table_t *table = &(db->tables[i]);
    free(table->word_start_offset);
    free(table->word_end_offset);
    free(table->blocks);
  }
  free(db);
}

int lookup_table(db_t *db, int table_index, int wid, int target_context_offset, int *return_offset) {
  table_t *table;
  block_t *blocks;
  int a,b,c;
  int word_offset_end, word_offset_begin;
  int pos;
  void *ptr;
  int block_offset, offset, end_offset, context_offset;

  table = &(db->tables[table_index]);
  blocks = table->blocks;

  if (wid > table->max_wid || wid < 0) { return -1; /* bad word ID */ }

  /* find the block containing the right word and context */
  a = 0;
  b = table->block_count - 1;

  word_offset_begin = table->word_start_offset[wid];
  word_offset_end = table->word_end_offset[wid];

  if (word_offset_begin == -1) { return -1; /* word not found */ }

  while (b > a + 1)  {
    int table_offset, context_offset;

    c = (a + b) / 2;
    table_offset = blocks[c].table_offset;
    context_offset = blocks[c].context_offset;

    if (word_offset_end < table_offset) { b = c; continue; }
    if (word_offset_begin >= table_offset) { a = c; continue; }

    /* our target line is in [a, b[ blocks (requested word spans across all these blocks) */

    if (context_offset > target_context_offset) {
      b = c;
    } else {
      a = c;
    }
  }

  /* parse block to find the line with the right word and context */
  ptr = blocks[a].data;
  pos = 0;
  block_offset = offset = blocks[a].table_offset;
  end_offset = blocks[a + 1].table_offset;
  context_offset = blocks[a].context_offset;

  while (offset < end_offset) {
    int cur_context_offset;
    int count;

    cur_context_offset = get_variable_length(db->base_bits, &ptr, &pos);
    count = get_variable_length(db->base_bits, &ptr, &pos);

    if (offset == block_offset) {
      cur_context_offset = context_offset; /* first block item is easy: just get context information from the block header */
    } else if (offset > word_offset_begin) {
      cur_context_offset += context_offset; /* we store context offset deltas (except for first context of each word) */
    }
    if (offset >= word_offset_begin && offset <= word_offset_end && cur_context_offset == target_context_offset) {
      if (return_offset) { *return_offset = offset; }
      return count;
    }

    if (offset > word_offset_end || (cur_context_offset > target_context_offset && offset >= word_offset_begin)) {
      return -1; /* oops we've gone too far */
    }

    offset ++;
    context_offset = cur_context_offset;
  }


  return -1;
}

int search(db_t *db, int wids[], int count) {
  int context = 0;
  int i;
  int result = -1;

  for(i = 0; i < count; i ++) {
    int new_context = 0;
    result = lookup_table(db, i, wids[i], context, &new_context);
    if (count == -1) { return -1; }
    context = new_context;
  }

  return result;
}


