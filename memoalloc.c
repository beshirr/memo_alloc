#include "memoalloc.h"
#include "internal.h"

static block_header* POOL_START = nullptr;
static block_header* POOL_END = nullptr;

static inline size_t block_size(const block_header* b) {
  return GET_SIZE(b->size_and_flags);
}

static inline int block_is_free(const block_header* b) {
  return IS_FREE(b->size_and_flags);
}

static inline int block_is_mmap(const block_header* b) {
  return IS_MMAP(b->size_and_flags);
}

static inline void mark_free(block_header* b) {
  SET_FREE(b->size_and_flags);
}

static inline void mark_used(block_header* b) {
  SET_USED(b->size_and_flags);
}

static block_header* find_block(size_t size) {
  block_header* cur = POOL_START;
  while (cur) {
    if (block_is_free(cur) && block_size(cur) >= size)
      return cur;
    cur = cur->next;
  }
  return nullptr;
}

static void split(block_header* b, size_t size) {
  size_t remainder = block_size(b) - size;

  if (remainder < sizeof(block_header) + ALIGN)
    return;

  auto newb = (block_header*)((char*)(b + 1) + size);

  newb->size_and_flags = remainder - sizeof(block_header);
  mark_free(newb);

  newb->next = b->next;
  newb->prev = b;

  if (newb->next)
    newb->next->prev = newb;
  else
    POOL_END = newb;

  b->next = newb;
  b->size_and_flags = size;
}

static void* place(block_header* b, size_t size) {
  split(b, size);
  mark_used(b);
  return (void*)(b + 1);
}

static block_header* coalesce(block_header* b) {
  if (b->next && block_is_free(b->next) && !block_is_mmap(b->next)) {
    const block_header * n = b->next;
    b->size_and_flags = block_size(b) + sizeof(block_header) + block_size(n);
    mark_free(b);

    b->next = n->next;
    if (n->next) n->next->prev = b;
    else POOL_END = b;
  }

  if (b->prev && block_is_free(b->prev) && !block_is_mmap(b->prev)) {
    b = b->prev;
    return coalesce(b);
  }

  return b;
}

static block_header* extend_pool(const long size) {
  long extend_size = ALIGN_UP(size + sizeof(block_header));
  if (extend_size < POOL_EXTEND_SIZE) {
    extend_size = POOL_EXTEND_SIZE;
  }
  void *p = sbrk(extend_size);
  if (p == (void *)-1) return nullptr;
  const auto block = (block_header*)p;
  block->size_and_flags = extend_size - sizeof(block_header);
  SET_FREE(block->size_and_flags);
  block->next = nullptr;
  block->prev = POOL_END;

  if (!POOL_START) POOL_START = block;
  else POOL_END->next = block;
  POOL_END = block;
  return block;
}

void* alloc(size_t size) {
  if (!size) return NULL;

  size = ALIGN_UP(size);

  block_header* b = find_block(size);
  if (b)
    return place(b, size);

  b = extend_pool((long)size);
  if (!b) return NULL;

  return place(b, size);
}

void free(void* ptr) {
  if (!ptr) return;

  block_header* b = (block_header*)ptr - 1;

  if (block_is_mmap(b)) {
    size_t total = block_size(b) + sizeof(block_header);
    munmap(b, total);
    return;
  }

  mark_free(b);
  coalesce(b);
}