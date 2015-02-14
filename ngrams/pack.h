#ifndef PACK_H
#define PACK_H

#include <stdint.h>

/* endianness independant serialization */

uint32_t get_uint32(void* ptr);
int32_t get_int32(void* ptr);
uint16_t get_uint16(void* ptr);
unsigned char get_char(void * ptr);
void set_uint32(void * ptr, uint32_t value);

#endif /* PACK_H */
