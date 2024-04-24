#ifndef UNTITLED_HEAP_H
#define UNTITLED_HEAP_H

#include <stdio.h>

struct memory_manager_t {
    void *memory_start;
    size_t memory_size;
    struct memory_chunk_t *first_memory_chunk;
};

struct memory_chunk_t {
    struct memory_chunk_t *prev;
    struct memory_chunk_t *next;
    size_t size;
    int free;
    int max_size;
    int when_user_block;
    int sum_check;
};

struct memory_manager_t memory_manager;

#define PAGE_SIZE 4096
#define FENCE_SIZE 2
#define ALIGNMENT 4
#define ALIGN(x) (((x) & ~(ALIGNMENT-1)) + ALIGNMENT* !!((x) & (ALIGNMENT-1)))

#define ALIGNMENT2 4096
#define ALIGN2(x) (((x) & ~(ALIGNMENT2-1)) + ALIGNMENT2* !!((x) & (ALIGNMENT2-1)))

void print_struct(void );

enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

void set_sum_check(struct memory_chunk_t *memoryChunk);
int check_fences(void *mem, size_t size);

int heap_setup(void);
void heap_clean(void);
int heap_validate(void);
void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);
size_t   heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);
void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);


#endif //UNTITLED_HEAP_H