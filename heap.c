#include "heap.h"
#include "custom_unistd.h"
#include <stdint.h>

enum pointer_type_t get_pointer_type(const void *const pointer) {
    if (pointer == NULL) {
        return pointer_null;
    }
    if (heap_validate()) {
        return pointer_heap_corrupted;
    }
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    if (temp == NULL) {
        return pointer_unallocated;
    }
    while (temp != NULL) {
        if ((void *) temp > pointer) {
            return pointer_unallocated;
        }
        if (temp == pointer) {
            return pointer_control_block;
        }
        if ((void *) ((uint8_t *) temp + sizeof(struct memory_chunk_t)) > pointer) {
            return pointer_control_block;
        }
        if((void *) ((uint8_t *) temp + sizeof(struct memory_chunk_t) + temp->when_user_block) > pointer){
            return pointer_unallocated; // miedzy struct a plotkami
        }
        if ((void *) ((uint8_t *) temp + sizeof(struct memory_chunk_t) + temp->when_user_block + FENCE_SIZE) > pointer) {
            if (temp->free) {
                return pointer_unallocated;
            }
            return pointer_inside_fences;
        }
        if ((void *) ((uint8_t *) temp + sizeof(struct memory_chunk_t) + temp->when_user_block + FENCE_SIZE) == pointer) {
            if (temp->free) {
                return pointer_unallocated;
            }
            return pointer_valid;
        }
        if ((void *) ((uint8_t *) temp + sizeof(struct memory_chunk_t) + temp->when_user_block + FENCE_SIZE + temp->size) > pointer) {
            if (temp->free) {
                return pointer_unallocated;
            }
            return pointer_inside_data_block;
        }
        if ((void *) ((uint8_t *) temp + sizeof(struct memory_chunk_t) + temp->when_user_block + FENCE_SIZE + temp->size + FENCE_SIZE) >
            pointer) {
            if (temp->free) {
                return pointer_unallocated;
            }
            return pointer_inside_fences;
        }
        temp = temp->next;
    }
    return pointer_unallocated;
}

int heap_setup(void) {
    if (memory_manager.memory_start) {
        return 0;
    }
    void *heap = custom_sbrk(0);
    if (heap != (void *) -1) {
        memory_manager.memory_start = heap;
        memory_manager.memory_size = 0;
        memory_manager.first_memory_chunk = NULL;
        return 0;
    } else {
        return -1;
    }
}

void heap_clean(void) {
    custom_sbrk((int)memory_manager.memory_size*-1);
    memory_manager.memory_start = NULL;
    memory_manager.memory_size = 0;
    memory_manager.first_memory_chunk = NULL;
}

int check_fences(void *mem, size_t size) {
    for (int i = 0; i < FENCE_SIZE; i++) {
        if (*((uint8_t *) mem + i) != '#') {
            return 1;
        }
    }
    for (int i = 0; i < FENCE_SIZE; i++) {
        if (*((uint8_t *) mem + i + FENCE_SIZE + size) != '#') {
            return 1;
        }
    }
    return 0;
}

int heap_validate(void) {
    if (memory_manager.memory_start == NULL) {
        return 2;
    }
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    while (temp != NULL) {
        int sum = 0;
        for (int i = 0; i < (int) (sizeof(struct memory_chunk_t)-sizeof(int)); i++) {
            sum += *((uint8_t *) temp + i);
        }
        if (sum != temp->sum_check) {
            return 3;
        }
        if (!temp->free) {
            if (check_fences((uint8_t *) temp + sizeof(struct memory_chunk_t)+ temp->when_user_block, temp->size)) {
                return 1;
            }
        }
        temp = temp->next;
    }
    return 0;
}

void set_sum_check(struct memory_chunk_t *memoryChunk) {
    if (memoryChunk == NULL) {
        return;
    }
    memoryChunk->sum_check = 0;
    for (int i = 0; i < (int) (sizeof(struct memory_chunk_t)-sizeof(int)); i++) {
        memoryChunk->sum_check += *((uint8_t *) memoryChunk + i);
    }
}

void *heap_malloc(size_t size) {
    if (size == 0 || heap_validate()) {
        return NULL;
    }
    if (memory_manager.first_memory_chunk == NULL) {
        int a = ALIGN((size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2));
        void *allocated = custom_sbrk(a);
        if (allocated == (void *) -1) {
            return NULL;
        }
        memory_manager.first_memory_chunk = allocated;
        memory_manager.memory_size = ALIGN((size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2));
        memory_manager.first_memory_chunk->size = size;
        memory_manager.first_memory_chunk->next = NULL;
        memory_manager.first_memory_chunk->prev = NULL;
        memory_manager.first_memory_chunk->free = 0;
        memory_manager.first_memory_chunk->max_size = ALIGN((size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2)) -
                                                      sizeof(struct memory_chunk_t) - FENCE_SIZE *2;
        memory_manager.first_memory_chunk->when_user_block = 0;
        memset((void *) ((uint8_t *) allocated + sizeof(struct memory_chunk_t)), '#', FENCE_SIZE);
        memset((void *) ((uint8_t *) allocated + sizeof(struct memory_chunk_t) + FENCE_SIZE + size), '#', FENCE_SIZE);
        set_sum_check(memory_manager.first_memory_chunk);
        return (uint8_t *) memory_manager.first_memory_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE;
    }
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    int8_t flag = 0;
    do {
        if (flag) {
            temp = temp->next;
        } else {
            flag = 1;
        }
        if (temp->free && size <= (unsigned long) (temp->max_size) ){
            temp->size = size;
            temp->free = 0;
            memset((void *) ((uint8_t *) temp + temp->when_user_block + sizeof(struct memory_chunk_t)), '#', FENCE_SIZE);
            memset((void *) ((uint8_t *) temp + temp->when_user_block + sizeof(struct memory_chunk_t) + FENCE_SIZE + size), '#', FENCE_SIZE);
            set_sum_check(temp);
            set_sum_check(temp->next);
            set_sum_check(temp->prev);
            return (uint8_t *) temp + temp->when_user_block + sizeof(struct memory_chunk_t) + FENCE_SIZE;
        }
    } while (temp->next != NULL);
    void *allocated = custom_sbrk(ALIGN((size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2)));
    if (allocated == (void *) -1) {
        return NULL;
    }
    ((struct memory_chunk_t *) allocated)->size = size;
    ((struct memory_chunk_t *) allocated)->max_size = ALIGN((size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2)) - sizeof(struct memory_chunk_t) - FENCE_SIZE *2;
    ((struct memory_chunk_t *) allocated)->next = NULL;
    ((struct memory_chunk_t *) allocated)->prev = temp;
    ((struct memory_chunk_t *) allocated)->free = 0;
    ((struct memory_chunk_t *) allocated)->when_user_block = 0;
    temp->next = allocated;
    memory_manager.memory_size += ALIGN((size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2));
    memset((void *) ((uint8_t *) allocated + sizeof(struct memory_chunk_t)), '#', FENCE_SIZE);
    memset((void *) ((uint8_t *) allocated + sizeof(struct memory_chunk_t) + FENCE_SIZE + size), '#', FENCE_SIZE);
    set_sum_check(allocated);
    set_sum_check(((struct memory_chunk_t *) allocated)->next);
    set_sum_check(((struct memory_chunk_t *) allocated)->prev);
    return (uint8_t *) allocated + sizeof(struct memory_chunk_t) + FENCE_SIZE;
}

struct memory_chunk_t *get_struct_from_valid_pointer(void *memblock){
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    while (temp){
        if((uint8_t*)temp+ temp->when_user_block+ sizeof(struct memory_chunk_t) + FENCE_SIZE == memblock){
            return temp;
        }
        temp=temp->next;
    }
    return NULL;
}

void heap_free(void *memblock) {
    if (memory_manager.first_memory_chunk == NULL || heap_validate() || get_pointer_type(memblock) != pointer_valid) {
        return;
    }
    struct memory_chunk_t *to_free = get_struct_from_valid_pointer(memblock);
    to_free->free = 1;
    while (to_free->prev && to_free->prev->free) {
        if (to_free->next) {
            to_free->next->prev = to_free->prev;
        }
        to_free->prev->max_size += to_free->max_size+ (int)sizeof(struct memory_chunk_t) + FENCE_SIZE *2;
        to_free->prev->size += to_free->max_size+ (int)sizeof(struct memory_chunk_t) + FENCE_SIZE *2;
        to_free->prev->next = to_free->next;
        if (to_free->prev == NULL) {
            break;
        }
        to_free = to_free->prev;
    }
    while (to_free->next && to_free->next->free) {
        to_free->next->next->prev = to_free;
        to_free->max_size += to_free->next->max_size + (int)sizeof(struct memory_chunk_t) + FENCE_SIZE * 2;
        to_free->size += to_free->next->max_size;
        to_free->next = to_free->next->next;
        if (to_free->next == NULL || !to_free->next->free) {
            break;
        }
        to_free = to_free->next;
    }
    set_sum_check(to_free);
    set_sum_check(to_free->next);
    set_sum_check(to_free->prev);
    if (to_free->next == NULL && to_free->prev == NULL) {
        memory_manager.first_memory_chunk = NULL;
        custom_sbrk((long) memory_manager.memory_size * -1);
        memory_manager.memory_size = 0;
    } else if (to_free->next == NULL) {
        struct memory_chunk_t *last = to_free->prev;
        memory_manager.memory_size -= to_free->max_size+ sizeof(struct memory_chunk_t) + to_free->when_user_block + FENCE_SIZE*2;
        custom_sbrk((int)(to_free->max_size+ sizeof(struct memory_chunk_t) + to_free->when_user_block + FENCE_SIZE*2) * -1);
        last->next = NULL;
        set_sum_check(last);
    }
}

void *heap_calloc(size_t number, size_t size) {
    if (number == 0 || size == 0) {
        return NULL;
    }
    void *res = heap_malloc(number * size);
    if (res) {
        memset(res, 0, size * number);
    }
    return res;
}

size_t heap_get_largest_used_block_size(void) {
    if (heap_validate() || memory_manager.first_memory_chunk == NULL || memory_manager.memory_start == NULL) {
        return 0;
    }
    size_t max_size = 0;
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    while (temp != NULL) {
        if (temp->size > max_size && !temp->free) {
            max_size = temp->size;
        }
        temp = temp->next;
    }
    return max_size;
}

void *heap_realloc(void *memblock, size_t count) {
    if (count == 0) {
        heap_free(memblock);
        return NULL;
    }
    if(memblock==NULL){
        return heap_malloc(count);
    }
    if(get_pointer_type(memblock)!=pointer_valid){
        return NULL;
    }
    struct memory_chunk_t *to_realloc = get_struct_from_valid_pointer(memblock);
    size_t mem_to_realloc_size = to_realloc->size;
    if((int)count<=to_realloc->max_size){
        memset((void *) ((uint8_t *) to_realloc + sizeof(struct memory_chunk_t)+ to_realloc->when_user_block + FENCE_SIZE + count), '#', FENCE_SIZE);
        to_realloc->size=count;
        set_sum_check(to_realloc);
        set_sum_check(to_realloc->next);
        set_sum_check(to_realloc->prev);
        return memblock;
    }
    if(to_realloc->next==NULL){
        if((int)count<=to_realloc->max_size){
            to_realloc->size = count;
            memset((void *) ((uint8_t *) to_realloc + sizeof(struct memory_chunk_t)+to_realloc->when_user_block + FENCE_SIZE + count), '#', FENCE_SIZE);
            set_sum_check(to_realloc);
            set_sum_check(to_realloc->next);
            set_sum_check(to_realloc->prev);
            return memblock;
        }
        if(custom_sbrk((long)ALIGN(count-to_realloc->max_size))==(void*)-1){
            return NULL;
        }
        memory_manager.memory_size += ALIGN(count-to_realloc->max_size);
        to_realloc->size = count;
        to_realloc->max_size += (int)ALIGN(count-to_realloc->max_size);
        set_sum_check(to_realloc);
        set_sum_check(to_realloc->next);
        set_sum_check(to_realloc->prev);
        memset((void *) ((uint8_t *) to_realloc + sizeof(struct memory_chunk_t)+ to_realloc->when_user_block + FENCE_SIZE + count), '#', FENCE_SIZE);
        return memblock;
    }
    void *allocated = custom_sbrk(ALIGN((count + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2)));
    if (allocated == (void *) -1) {
        return NULL;
    }
    struct memory_chunk_t *copy = ((struct memory_chunk_t *) allocated);
    memory_manager.memory_size += ALIGN((count + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2));
    struct memory_chunk_t *var = memory_manager.first_memory_chunk;
    while (var->next!=NULL){
        var=var->next;
    }
    copy->max_size = ALIGN((count + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2)) - sizeof(struct memory_chunk_t) - FENCE_SIZE *2;
    copy->when_user_block = 0;
    copy->size = count;
    copy->next = NULL;
    copy->prev = var;
    copy->free = 0;
    var->next = copy;
    memset((void *) ((uint8_t *) copy + sizeof(struct memory_chunk_t)), '#', FENCE_SIZE);
    memset((void *) ((uint8_t *) copy + sizeof(struct memory_chunk_t) + FENCE_SIZE + count), '#', FENCE_SIZE);
    set_sum_check(copy);
    set_sum_check(var);
    memcpy((uint8_t *) copy + sizeof(struct memory_chunk_t) + FENCE_SIZE,memblock,mem_to_realloc_size);
    heap_free(memblock);
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    int8_t flag = 0;
    do {
        if (flag) {
            temp = temp->next;
        } else {
            flag = 1;
        }
        if (temp->free && count <= (unsigned long) (temp->max_size)){
            temp->size = count;
            temp->free = 0;
            memcpy((uint8_t *) temp + sizeof(struct memory_chunk_t) + temp->when_user_block + FENCE_SIZE,(uint8_t *) copy + sizeof(struct memory_chunk_t) + FENCE_SIZE,mem_to_realloc_size);
            memset((void *) ((uint8_t *) temp + temp->when_user_block + sizeof(struct memory_chunk_t)), '#', FENCE_SIZE);
            memset((void *) ((uint8_t *) temp + temp->when_user_block +sizeof(struct memory_chunk_t) + FENCE_SIZE + count), '#', FENCE_SIZE);
            set_sum_check(temp);
            set_sum_check(temp->next);
            set_sum_check(temp->prev);
            heap_free((uint8_t *) copy + sizeof(struct memory_chunk_t) + FENCE_SIZE);
            return (uint8_t *) temp + temp->when_user_block+ sizeof(struct memory_chunk_t) + FENCE_SIZE;
        }
    } while (temp->next != NULL);
    return (uint8_t *) allocated + sizeof(struct memory_chunk_t) + FENCE_SIZE;
}

void* heap_malloc_aligned(size_t count){
    if (count == 0 || heap_validate()) {
        return NULL;
    }
    if (memory_manager.first_memory_chunk == NULL) {
        void *allocated = custom_sbrk(ALIGN2((count + FENCE_SIZE))+PAGE_SIZE);
        if (allocated == (void *) -1) {
            return NULL;
        }
        memory_manager.first_memory_chunk = allocated;
        memory_manager.memory_size = ALIGN2((count + FENCE_SIZE))+PAGE_SIZE;
        memory_manager.first_memory_chunk->size = count;
        memory_manager.first_memory_chunk->next = NULL;
        memory_manager.first_memory_chunk->prev = NULL;
        memory_manager.first_memory_chunk->free = 0;
        memory_manager.first_memory_chunk->max_size = ALIGN2((count + FENCE_SIZE))-FENCE_SIZE;
        memory_manager.first_memory_chunk->when_user_block = PAGE_SIZE- sizeof(struct memory_chunk_t)-FENCE_SIZE;
        memset((void *) ((uint8_t *) allocated + PAGE_SIZE - FENCE_SIZE), '#', FENCE_SIZE);
        memset((void *) ((uint8_t *) allocated + PAGE_SIZE + count), '#', FENCE_SIZE);
        set_sum_check(memory_manager.first_memory_chunk);
        return (uint8_t *) memory_manager.first_memory_chunk + PAGE_SIZE;
    }
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    int8_t flag = 0;
    do {
        if (flag) {
            temp = temp->next;
        } else {
            flag = 1;
        }
        if (temp->free && count<=(unsigned long) (temp->max_size) && (temp->when_user_block)!=0){
            temp->size = count;
            temp->free = 0;
            memset((void *) ((uint8_t *) temp + PAGE_SIZE - FENCE_SIZE), '#', FENCE_SIZE);
            memset((void *) ((uint8_t *) temp + PAGE_SIZE + count), '#', FENCE_SIZE);
            set_sum_check(temp);
            set_sum_check(temp->next);
            set_sum_check(temp->prev);
            return (uint8_t *) temp + PAGE_SIZE;
        }
    } while (temp->next != NULL);
    void *ptr = custom_sbrk(0);
    while (((intptr_t)ptr & (intptr_t)(PAGE_SIZE - 1)) != 0){
        ptr = custom_sbrk(1);
        if(ptr==(void*)-1){
            return NULL;
        }
        temp->max_size+=1;
        memory_manager.memory_size+=1;
        ptr = custom_sbrk(0);
    }
    void *allocated = custom_sbrk(ALIGN2(count + FENCE_SIZE + PAGE_SIZE));
    if (allocated == (void *) -1) {
        return NULL;
    }
    ((struct memory_chunk_t *) allocated)->size = count;
    ((struct memory_chunk_t *) allocated)->max_size = ALIGN2(count+FENCE_SIZE)-FENCE_SIZE;
    ((struct memory_chunk_t *) allocated)->next = NULL;
    ((struct memory_chunk_t *) allocated)->prev = temp;
    ((struct memory_chunk_t *) allocated)->free = 0;
    ((struct memory_chunk_t *) allocated)->when_user_block = PAGE_SIZE- sizeof(struct memory_chunk_t)-FENCE_SIZE;
    temp->next = allocated;
    memory_manager.memory_size += ALIGN2(count + FENCE_SIZE + PAGE_SIZE);
    memset((void *) ((uint8_t *) allocated +  PAGE_SIZE - FENCE_SIZE), '#', FENCE_SIZE);
    memset((void *) ((uint8_t *) allocated + PAGE_SIZE + count), '#', FENCE_SIZE);
    set_sum_check(allocated);
    set_sum_check(((struct memory_chunk_t *) allocated)->next);
    set_sum_check(((struct memory_chunk_t *) allocated)->prev);
    return (uint8_t *) allocated + PAGE_SIZE;
}

void* heap_calloc_aligned(size_t number, size_t size){
    if (number == 0 || size == 0) {
        return NULL;
    }
    void *res = heap_malloc_aligned(number * size);
    if (res) {
        memset(res, 0, size * number);
    }
    return res;
}

void* heap_realloc_aligned(void* memblock, size_t size){
    if (size == 0) {
        heap_free(memblock);
        return NULL;
    }
    if(memblock==NULL){
        return heap_malloc_aligned(size);
    }
    if(get_pointer_type(memblock)!=pointer_valid){
        return NULL;
    }
    struct memory_chunk_t *to_realloc = get_struct_from_valid_pointer(memblock);
    size_t mem_to_realloc_size = to_realloc->size;
    if((int)size<=to_realloc->max_size){
        memset((uint8_t *) memblock + size, '#', FENCE_SIZE);
        to_realloc->size=size;
        set_sum_check(to_realloc);
        set_sum_check(to_realloc->next);
        set_sum_check(to_realloc->prev);
        return memblock;
    }
    if(to_realloc->next==NULL){
        if(custom_sbrk(ALIGN2(size-to_realloc->max_size))==(void*)-1){
            return NULL;
        }
        memory_manager.memory_size += ALIGN2(size-to_realloc->max_size);
        to_realloc->size = size;
        set_sum_check(to_realloc);
        set_sum_check(to_realloc->next);
        set_sum_check(to_realloc->prev);
        memset((uint8_t *) memblock + size, '#', FENCE_SIZE);
        return memblock;
    }
    struct memory_chunk_t *var = memory_manager.first_memory_chunk;
    while (var->next!=NULL){
        var=var->next;
    }
    void *ptr = custom_sbrk(0);
    while (((intptr_t)ptr & (intptr_t)(PAGE_SIZE - 1)) != 0){
        ptr = custom_sbrk(1);
        if(ptr==(void*)-1){
            return NULL;
        }
        var->max_size+=1;
        memory_manager.memory_size+=1;
        ptr = custom_sbrk(0);
    }
    void *allocated = custom_sbrk(ALIGN2(size + FENCE_SIZE)+PAGE_SIZE);
    if (allocated == (void *) -1) {
        return NULL;
    }
    struct memory_chunk_t *copy = ((struct memory_chunk_t *) allocated);
    memory_manager.memory_size += ALIGN2(size + FENCE_SIZE)+PAGE_SIZE;
    copy->max_size = ALIGN2(size + FENCE_SIZE)-FENCE_SIZE;
    copy->when_user_block = PAGE_SIZE - FENCE_SIZE - sizeof(struct memory_chunk_t);
    copy->size = size;
    copy->next = NULL;
    copy->prev = var;
    copy->free = 0;
    var->next = copy;
    memset((uint8_t *) copy + sizeof(struct memory_chunk_t) + copy->when_user_block, '#', FENCE_SIZE);
    memset((uint8_t *) copy + sizeof(struct memory_chunk_t) + copy->when_user_block + FENCE_SIZE + size, '#', FENCE_SIZE);
    set_sum_check(copy);
    set_sum_check(var);
    memcpy((uint8_t *) copy + sizeof(struct memory_chunk_t) + copy->when_user_block + FENCE_SIZE,memblock,mem_to_realloc_size);
    heap_free(memblock);
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;
    int8_t flag = 0;
    do {
        if (flag) {
            temp = temp->next;
        } else {
            flag = 1;
        }
        if (temp->free && (size <= (unsigned long) (temp->max_size)) && (temp->when_user_block)!=0) {
            temp->size = size;
            temp->free = 0;
            memcpy((uint8_t *) temp + temp->when_user_block+ sizeof(struct memory_chunk_t) + FENCE_SIZE,(uint8_t *) copy+copy->when_user_block + sizeof(struct memory_chunk_t) + FENCE_SIZE,mem_to_realloc_size);
            memset((uint8_t *) temp + sizeof(struct memory_chunk_t) + temp->when_user_block, '#', FENCE_SIZE);
            memset((uint8_t *) temp + sizeof(struct memory_chunk_t) + FENCE_SIZE + temp->when_user_block+ size, '#', FENCE_SIZE);
            set_sum_check(temp);
            set_sum_check(temp->next);
            set_sum_check(temp->prev);
            heap_free((uint8_t *) copy + copy->when_user_block +sizeof(struct memory_chunk_t) + FENCE_SIZE);
            return (uint8_t *) temp +temp->when_user_block + sizeof(struct memory_chunk_t) + FENCE_SIZE;
        }
    } while (temp->next != NULL);
    return (uint8_t *) copy + sizeof(struct memory_chunk_t) + copy->when_user_block + FENCE_SIZE;
}