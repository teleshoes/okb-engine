/* endianness independant serialization */

#include "pack.h"

uint32_t get_uint32(void* ptr) {
  unsigned char* np = (unsigned char*) ptr;
  return ((uint32_t)np[0] << 24) |
    ((uint32_t)np[1] << 16) |
    ((uint32_t)np[2] << 8) |
    (uint32_t)np[3];
}

int32_t get_int32(void* ptr) {
  return (int32_t) get_uint32(ptr);
}

uint16_t get_uint16(void* ptr) {
  unsigned char* np = (unsigned char*) ptr;
  return ((uint32_t)np[0] << 8) |
    (uint32_t)np[1];
}

unsigned char get_char(void * ptr) {
  return *((unsigned char*) ptr);
}

void set_uint32(void * ptr, uint32_t value) {
  unsigned char* cptr = ptr;
  *(cptr ++) = value >> 24;
  *(cptr ++) = (value >> 16) & 0xff;
  *(cptr ++) = (value >> 8) & 0xff;
  *(cptr ++) = value & 0xff;
}  
