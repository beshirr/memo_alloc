#ifndef INTERNAL_H
#define INTERNAL_H

#include <stddef.h>

#define INITIAL_POOL_SIZE (1024 * 1024)
#define THRESHOLD (128 * 1024)
#define POOL_EXTEND_SIZE (1024 * 1024)

#define ALIGN 16
#define ALIGN_UP(size) (((size) + (ALIGN - 1)) & ~(ALIGN - 1))

#define FREE_MASK 0x1
#define MMAP_MASK 0x2
#define FLAGS_MASK 0xF

#define GET_SIZE(x) ((x) & ~FLAGS_MASK)
#define IS_FREE(x)  ((x) & FREE_MASK)
#define IS_MMAP(x)  ((x) & MMAP_MASK)

#define SET_FREE(x) ((x) |= FREE_MASK)
#define SET_USED(x) ((x) &= ~FREE_MASK)
#define SET_MMAP(x) ((x) |= MMAP_MASK)

typedef struct block_header {
  size_t size_and_flags;
  struct block_header* next;
  struct block_header* prev;
} block_header;

#endif