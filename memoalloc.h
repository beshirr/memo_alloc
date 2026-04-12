#ifndef MALLOC_H
#define MALLOC_H

#include <sys/mman.h>
#include <unistd.h>

void* alloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size); // later

#endif