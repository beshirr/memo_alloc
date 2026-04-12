#include <sys/mman.h>
#include <unistd.h>

#define INITIAL_POOL_SIZE (1024 * 1024)
#define THRESHOLD (128 * 1024)
#define POOL_EXTEND_SIZE (1024 * 1024)
#define ALIGN 16
#define ALIGN_UP(size) (((size) + (ALIGN - 1)) & ~(ALIGN - 1))
#define FREE_MASK 0x1
#define FLAGS_MASK 0xF
#define GET_SIZE(size) ((size) & ~FLAGS_MASK)
#define IS_FREE(size)  ((size) & FREE_MASK)
#define SET_FREE(size)  ((size) |= FREE_MASK)
#define SET_USED(size)  ((size) &= ~FREE_MASK)
#define MMAP_MASK 0x2
#define IS_MMAP(x) ((x) & MMAP_MASK)
#define SET_MMAP(x) ((x) |= MMAP_MASK)
#define CLEAR_MMAP(x) ((x) &= ~MMAP_MASK)


typedef struct block_header {
  size_t size_f;
  struct block_header* next;
  struct block_header* prev;
} block_header;


static block_header* POOL_START = nullptr;
static block_header* POOL_END = nullptr;


void init_pool() {
  void* p = sbrk(INITIAL_POOL_SIZE);
  if (p == (void *)-1) return;
  POOL_START = (block_header *)p;
  POOL_START->size_f = INITIAL_POOL_SIZE - sizeof(block_header);
  SET_FREE(POOL_START->size_f);
  POOL_START->next = nullptr;
  POOL_START->prev = nullptr;
  POOL_END = POOL_START;
}


static block_header* extend_pool(const long size) {
  long extend_size = ALIGN_UP(size + sizeof(block_header));
  if (extend_size < POOL_EXTEND_SIZE) {
    extend_size = POOL_EXTEND_SIZE;
  }
  void *p = sbrk(extend_size);
  if (p == (void *)-1) return nullptr;
  const auto block = (block_header*)p;
  block->size_f = extend_size - sizeof(block_header);
  SET_FREE(block->size_f);
  block->next = nullptr;
  block->prev = POOL_END;

  if (!POOL_START) POOL_START = block;
  else POOL_END->next = block;
  POOL_END = block;
  return block;
}


static void* mmap_alloc(size_t size) {
  size = ALIGN_UP(size + sizeof(block_header));

  void* p = mmap(NULL, size,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1, 0);
  if (p == MAP_FAILED) return nullptr;

  const auto header = (block_header*)p;
  header->size_f = size - sizeof(block_header);
  SET_USED(header->size_f);
  SET_MMAP(header->size_f);
  header->next = nullptr;
  header->prev = POOL_END;

  if (!POOL_START) POOL_START = header;
  else POOL_END->next = header;
  POOL_END = header;

  return (void *)(header + 1);
}


void split_chunk(block_header* chunk, size_t size, size_t remainder) {
  if (chunk == nullptr) return;
  const auto block = (block_header*)((char*)(chunk+1)+size);
  block->size_f = remainder - sizeof(block_header);
  SET_FREE(block->size_f);
  block->next = chunk->next;
  block->prev = chunk;
  if (block->next != nullptr) {
    block->next->prev = block;
  } else {
    POOL_END = block;
  }
  chunk->next = block;
  chunk->size_f = size;
  SET_USED(chunk->size_f);
}


void* alloc(size_t size) {
  if (POOL_START == NULL)
    init_pool();

  size = ALIGN_UP(size);

  if (size >= THRESHOLD)
    return mmap_alloc(size);

  block_header* current = POOL_START;

  while (current != NULL) {
    if (IS_FREE(current->size_f) && GET_SIZE(current->size_f) >= size) {
      const size_t remainder = GET_SIZE(current->size_f) - size;
      if (remainder >= sizeof(block_header) + ALIGN) {
        split_chunk(current, size, remainder);
      }
      return (void*) (current+1);
    }
    current = current->next;
  }

  auto *header = extend_pool((long)size);
  if (header == nullptr) return nullptr;
  const size_t remainder = GET_SIZE(header->size_f) - size;
  if (remainder >= sizeof(block_header) + ALIGN) {
    split_chunk(header, size, remainder);
  }
  SET_USED(header->size_f);
  return (void* ) (header + 1);
}


void coalesce() {
  block_header* current = POOL_START;

  while (current != NULL) {
    if (IS_FREE(current->size_f) && !IS_MMAP(current->size_f)) {
      while (current->next &&
             IS_FREE(current->next->size_f) &&
             !IS_MMAP(current->next->size_f)) {

        block_header* next = current->next;

        current->size_f = GET_SIZE(current->size_f) +
                          sizeof(block_header) +
                          GET_SIZE(next->size_f);
        SET_FREE(current->size_f);

        current->next = next->next;

        if (next->next)
          next->next->prev = current;
        else
          POOL_END = current;
             }
    }
    current = current->next;
  }
}


void free(void* p) {
  const auto header = (block_header*)p - 1;
  if (IS_MMAP(header->size_f)) {
    const size_t total_size = GET_SIZE(header->size_f) + sizeof(block_header);
    munmap(header, total_size);
    return;
  }
  SET_FREE(header->size_f);

  coalesce();
}

int main() {

}
