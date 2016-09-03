//Header	
#ifndef MEMALLOC_H
#define MEMALLOC_H

#define BLOCK_SIZE 500
#define NUMBER_POINTERS 10

void* my_malloc(int size);
void my_free(void *ptr);
void my_mallopt(int policy);
void my_mallinfo();
extern char *my_malloc_error();

#endif
