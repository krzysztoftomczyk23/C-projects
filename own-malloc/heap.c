#include "heap.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"
#define FENCE_SIZE 16


enum pointer_type_t get_pointer_type(const void* const pointer){
    if(pointer == NULL){
        return pointer_null;
    }
    if(heap_validate() != 0){
        return pointer_heap_corrupted;
    }
    struct memory_chunk_t * cur_chunk = memory_manager.first_memory_chunk;
    int8_t * mem_pointer;
    while (cur_chunk != NULL){
        mem_pointer = (int8_t *) cur_chunk;
        if(cur_chunk->free == 1){
            for (int i = 0; i < (int)sizeof(struct memory_chunk_t); ++i) {
                if(mem_pointer == pointer){
                    return pointer_control_block;
                }
                mem_pointer++;
            }
            for (int i = 0; i < (int)cur_chunk->size; ++i) {
                if(mem_pointer == pointer){
                    return pointer_unallocated;
                }
                mem_pointer++;
            }
            cur_chunk = cur_chunk->next;
            continue;
        }

        for (int i = 0; i < (int)sizeof(struct memory_chunk_t); ++i) {
            if(mem_pointer == pointer){
                return pointer_control_block;
            }
            mem_pointer++;
        }
        for (int i = 0; i < FENCE_SIZE; ++i) {
            if(mem_pointer == pointer){
                return pointer_inside_fences;
            }
            mem_pointer++;
        }
        if(mem_pointer == pointer){
            return pointer_valid;
        }
        mem_pointer++;
        for (int i = 0; i < (int)cur_chunk->size-1; ++i) {
            if(mem_pointer == pointer){
                return pointer_inside_data_block;
            }
            mem_pointer++;
        }
        for (int i = 0; i < FENCE_SIZE; ++i) {
            if(mem_pointer == pointer){
                return pointer_inside_fences;
            }
            mem_pointer++;
        }



        cur_chunk = cur_chunk->next;
    }
    return pointer_unallocated;
}



int heap_validate(void){
    if(memory_manager.memory_start == NULL){
        return 2;
    }
    struct memory_chunk_t * cur_chunk = memory_manager.first_memory_chunk;
    int8_t * start_mem = memory_manager.memory_start;
    int8_t * end_mem = start_mem + memory_manager.memory_size;
    int size_of_mem = 0;
    while(cur_chunk != NULL){
        int8_t * cur_chunk_pointer = (int8_t *) cur_chunk;
        if(cur_chunk_pointer < start_mem || cur_chunk_pointer + sizeof(struct memory_chunk_t)> end_mem){
            return 3;
        }
        int8_t * testing_pointer = (int8_t *) cur_chunk->prev;
        if(testing_pointer != NULL){
            if(testing_pointer < start_mem || testing_pointer + sizeof(struct memory_chunk_t)> end_mem){
                return 3;
            }
        }
        testing_pointer = (int8_t *) cur_chunk->next;
        if(testing_pointer != NULL){
            if(testing_pointer < start_mem || testing_pointer + sizeof(struct memory_chunk_t)> end_mem){
                return 3;
            }
        }

        if(cur_chunk->next != NULL){
            if(cur_chunk->next->prev != cur_chunk){
                return 3;
            }
        }
        if(cur_chunk->prev != NULL){
            if(cur_chunk->prev->next != cur_chunk){
                return 3;
            }
        }
        if(cur_chunk->free != 0 && cur_chunk->free != 1){
            return 3;
        }
        size_of_mem += sizeof(struct memory_chunk_t);
        if(cur_chunk->free == 1){
            size_of_mem += (int)cur_chunk->size;
        }
        if(cur_chunk->free == 0){
            size_of_mem += 2*FENCE_SIZE;
            size_of_mem+=(int)cur_chunk->size;
        }
        if(cur_chunk->free == 0){
            int8_t * mem_pointer = (int8_t *) cur_chunk;
            mem_pointer+=(int) sizeof(struct memory_chunk_t);
            for (int i = 0; i < FENCE_SIZE; ++i) {
                if(mem_pointer > end_mem){
                    return 3;
                }
                if(*mem_pointer != 1){
                    return 1;
                }
                mem_pointer++;
            }
            mem_pointer += cur_chunk->size;
            for (int i = 0; i < FENCE_SIZE; ++i) {
                if(mem_pointer > end_mem){
                    return 3;
                }
                if(*mem_pointer != 1){
                    return 1;
                }
                mem_pointer++;
            }
        }
        cur_chunk = cur_chunk->next;
    }
    int memory_size = (int)memory_manager.memory_size;
    if(size_of_mem > memory_size){
        return 3;
    }
    return 0;
}


int heap_setup(void){
    void * start_addres = custom_sbrk(0);
    if(start_addres == (void *)-1){
        return -1;
    }
    memory_manager.memory_start = start_addres;
    memory_manager.first_memory_chunk = NULL;
    memory_manager.memory_size = 0;
    return 0;
}

void heap_clean(void){
    custom_sbrk(-memory_manager.memory_size);
    memory_manager.first_memory_chunk = NULL;
    memory_manager.memory_start = NULL;
}


void* heap_malloc(size_t size){
    if(size<= 0){
        return NULL;
    }
    if(heap_validate() != 0){
        return NULL;
    }
    if(memory_manager.first_memory_chunk == NULL){
        if(memory_manager.memory_size < size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE)){
            void * new_addres = custom_sbrk(size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE));
            if(new_addres == (void *)-1){
                return NULL;
            }
            memory_manager.memory_size += size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE);
        }
        memory_manager.first_memory_chunk = memory_manager.memory_start;
        struct memory_chunk_t * new_chunk = memory_manager.first_memory_chunk;
        new_chunk->size = size;
        new_chunk->free = 0;
        new_chunk->prev = NULL;
        new_chunk->next = NULL;
        int8_t * mem_pointer = (int8_t *) new_chunk;
        mem_pointer += sizeof(struct memory_chunk_t);
        for (int i = 0; i < FENCE_SIZE; ++i) {
            *mem_pointer = 1;
            mem_pointer++;
        }
        mem_pointer+=size;
        for (int i = 0; i < FENCE_SIZE; ++i) {
            *mem_pointer = 1;
            mem_pointer++;
        }
        mem_pointer = (int8_t *) new_chunk;
        mem_pointer += sizeof(struct memory_chunk_t);
        mem_pointer += FENCE_SIZE;
        return mem_pointer;
    }
    struct memory_chunk_t * cur_chunk = memory_manager.first_memory_chunk;
    while (1){
        if(cur_chunk->free == 1){
            if(cur_chunk->size >= (size+2*FENCE_SIZE)){
                cur_chunk->free = 0;
                int8_t * mem_pointer = (int8_t *) cur_chunk;
                mem_pointer += sizeof(struct memory_chunk_t);
                for (int i = 0; i < FENCE_SIZE; ++i) {
                    *mem_pointer = 1;
                    mem_pointer++;
                }
                mem_pointer+=size;
                for (int i = 0; i < FENCE_SIZE; ++i) {
                    *mem_pointer = 1;
                    mem_pointer++;
                }
                cur_chunk->size = size;
                mem_pointer = (int8_t *) cur_chunk;
                mem_pointer += sizeof(struct memory_chunk_t);
                mem_pointer += FENCE_SIZE;
                return mem_pointer;
            }
        }
        if(cur_chunk->next == NULL){
            break;
        }
        cur_chunk = cur_chunk->next;
    }
    int8_t * mem_used = memory_manager.memory_start;
    int space_used = 0;
    while(mem_used != (int8_t *)cur_chunk){
        mem_used++;
        space_used++;
    }
    space_used+= sizeof(struct memory_chunk_t);
    space_used+= 2*FENCE_SIZE;
    space_used+=(int)cur_chunk->size;
    mem_used+= sizeof(struct memory_chunk_t);
    mem_used+= 2*FENCE_SIZE;
    mem_used+=(int)cur_chunk->size;
    if(memory_manager.memory_size-space_used < size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE)){
        void * new_addres = custom_sbrk(size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE));
        if(new_addres == (void *)-1){
            return NULL;
        }
        memory_manager.memory_size += size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE);
    }
    struct memory_chunk_t * new_chunk = (struct memory_chunk_t *) mem_used;
    new_chunk->size = size;
    new_chunk->free = 0;
    new_chunk->prev = cur_chunk;
    cur_chunk->next = new_chunk;
    new_chunk->next = NULL;
    int8_t * mem_pointer = (int8_t *) new_chunk;
    mem_pointer += sizeof(struct memory_chunk_t);
    for (int i = 0; i < FENCE_SIZE; ++i) {
        *mem_pointer = 1;
        mem_pointer++;
    }
    mem_pointer+=size;
    for (int i = 0; i < FENCE_SIZE; ++i) {
        *mem_pointer = 1;
        mem_pointer++;
    }
    mem_pointer = (int8_t *) new_chunk;
    mem_pointer += sizeof(struct memory_chunk_t);
    mem_pointer += FENCE_SIZE;
    return mem_pointer;
}

void* heap_calloc(size_t number, size_t size){
    if(size<= 0 || number == 0){
        return NULL;
    }
    if(heap_validate() != 0){
        return NULL;
    }
    if(memory_manager.first_memory_chunk == NULL){
        if(memory_manager.memory_size<(size*number)+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE)){
            void * new_addres = custom_sbrk((size*number)+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE));
            if(new_addres == (void *)-1){
                return NULL;
            }
            memory_manager.memory_size += (number*size)+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE);
        }
        memory_manager.first_memory_chunk = memory_manager.memory_start;
        struct memory_chunk_t * new_chunk = memory_manager.first_memory_chunk;
        new_chunk->size = (number*size);
        new_chunk->free = 0;
        new_chunk->prev = NULL;
        new_chunk->next = NULL;
        int8_t * mem_pointer = (int8_t *) new_chunk;
        mem_pointer += sizeof(struct memory_chunk_t);
        for (int i = 0; i < FENCE_SIZE; ++i) {
            *mem_pointer = 1;
            mem_pointer++;
        }
        for (int i = 0; i < (int)(number*size); ++i) {
            *mem_pointer = 0;
            mem_pointer++;
        }
        for (int i = 0; i < FENCE_SIZE; ++i) {
            *mem_pointer = 1;
            mem_pointer++;
        }
        mem_pointer = (int8_t *) new_chunk;
        mem_pointer += sizeof(struct memory_chunk_t);
        mem_pointer += FENCE_SIZE;
        return mem_pointer;
    }
    struct memory_chunk_t * cur_chunk = memory_manager.first_memory_chunk;
    while (1){
        if(cur_chunk->free == 1){
            if(cur_chunk->size >= ((number*size)+2*FENCE_SIZE)){
                cur_chunk->free = 0;
                int8_t * mem_pointer = (int8_t *) cur_chunk;
                mem_pointer += sizeof(struct memory_chunk_t);
                for (int i = 0; i < FENCE_SIZE; ++i) {
                    *mem_pointer = 1;
                    mem_pointer++;
                }
                for (int i = 0; i < (int)(number*size); ++i) {
                    *mem_pointer = 0;
                    mem_pointer++;
                }
                for (int i = 0; i < FENCE_SIZE; ++i) {
                    *mem_pointer = 1;
                    mem_pointer++;
                }
                cur_chunk->size = (number*size);
                mem_pointer = (int8_t *) cur_chunk;
                mem_pointer += sizeof(struct memory_chunk_t);
                mem_pointer += FENCE_SIZE;
                return mem_pointer;
            }
        }
        if(cur_chunk->next == NULL){
            break;
        }
        cur_chunk = cur_chunk->next;
    }
    int8_t * mem_used = memory_manager.memory_start;
    int space_used = 0;
    while(mem_used != (int8_t *)cur_chunk){
        mem_used++;
        space_used++;
    }
    space_used+= sizeof(struct memory_chunk_t);
    space_used+= 2*FENCE_SIZE;
    space_used+=(int)cur_chunk->size;
    mem_used+= sizeof(struct memory_chunk_t);
    mem_used+= 2*FENCE_SIZE;
    mem_used+=(int)cur_chunk->size;
    if(memory_manager.memory_size-space_used < (number*size)+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE)){
        void * new_addres = custom_sbrk((number*size)+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE));
        if(new_addres == (void *)-1){
            return NULL;
        }
        memory_manager.memory_size += (number*size)+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE);
    }
    struct memory_chunk_t * new_chunk = (struct memory_chunk_t *) mem_used;
    new_chunk->size = (number*size);
    new_chunk->free = 0;
    new_chunk->prev = cur_chunk;
    cur_chunk->next = new_chunk;
    new_chunk->next = NULL;
    int8_t * mem_pointer = (int8_t *) new_chunk;
    mem_pointer += sizeof(struct memory_chunk_t);
    for (int i = 0; i < FENCE_SIZE; ++i) {
        *mem_pointer = 1;
        mem_pointer++;
    }
    for (int i = 0; i < (int)(number*size); ++i) {
        *mem_pointer = 0;
        mem_pointer++;
    }
    for (int i = 0; i < FENCE_SIZE; ++i) {
        *mem_pointer = 1;
        mem_pointer++;
    }
    mem_pointer = (int8_t *) new_chunk;
    mem_pointer += sizeof(struct memory_chunk_t);
    mem_pointer += FENCE_SIZE;
    return mem_pointer;
}


void* heap_realloc(void* memblock, size_t size){
    if(heap_validate() != 0){
        return NULL;
    }
    if(memblock == NULL){
        return heap_malloc(size);
    }
    if(size == 0){
        heap_free(memblock);
        return NULL;
    }
    if(get_pointer_type(memblock) != pointer_valid){
        return NULL;
    }
    int8_t * mem_pointer = memblock;
    mem_pointer-=FENCE_SIZE;
    mem_pointer-= sizeof(struct memory_chunk_t);
    struct memory_chunk_t * cur_chunk = (struct memory_chunk_t *) mem_pointer;
    if(cur_chunk->size >= size){
        mem_pointer = memblock;
        mem_pointer+= size;
        for (int i = 0; i < FENCE_SIZE; ++i) {
            *mem_pointer = 1;
            mem_pointer++;
        }
        cur_chunk->size = size;
        return memblock;
    }
    if(cur_chunk->next == NULL){
        int8_t * mem_counter = memory_manager.memory_start;
        int mem_size = 0;
        while(mem_counter != mem_pointer){
            mem_size++;
            mem_counter++;
        }
        mem_size+= (int)sizeof(struct memory_chunk_t);
        mem_size+=(2*FENCE_SIZE);
        mem_size+=(int)cur_chunk->size;
        int free_mem = (int)memory_manager.memory_size - mem_size;
        int mem_needed = (int)size - (int)cur_chunk->size;
        if(free_mem < mem_needed){
            void * new_addres = custom_sbrk(mem_needed);
            if(new_addres == (void *)-1){
                return NULL;
            }
            memory_manager.memory_size += mem_needed;
        }
        mem_pointer = memblock;
        mem_pointer+=size;
        for (int i = 0; i < FENCE_SIZE; ++i) {
            *mem_pointer = 1;
            mem_pointer++;
        }
        cur_chunk->size = size;
        return memblock;
    }
    if(cur_chunk->next->free == 1){
        int memory_haven = 0;
        int8_t * mem_counter = (int8_t *) cur_chunk;
        mem_counter+= sizeof(struct memory_chunk_t);
        while (mem_counter != (int8_t *)cur_chunk->next){
            mem_counter++;
            memory_haven++;
        }
        memory_haven += (int)cur_chunk->next->size;
        memory_haven += (int)sizeof(struct memory_chunk_t);
        memory_haven-=2*FENCE_SIZE;

        if(memory_haven >= (int)size){
            struct memory_chunk_t * next_chunk = cur_chunk->next->next;
            mem_pointer = memblock;
            mem_pointer+=size;
            for (int i = 0; i < FENCE_SIZE; ++i) {
                *mem_pointer = 1;
                mem_pointer++;
            }
            cur_chunk->size = size;
            cur_chunk->next = next_chunk;
            next_chunk->prev = cur_chunk;
            return memblock;
        }
    }
    void * new_addres = custom_sbrk(size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE));
    if(new_addres == (void *)-1){
        return NULL;
    }
    int8_t * mem_start = (int8_t *) memory_manager.memory_start;
    mem_start += memory_manager.memory_size;
    struct memory_chunk_t * new_chunk = (struct memory_chunk_t *) mem_start;
    memory_manager.memory_size += size+ sizeof(struct memory_chunk_t) + (2*FENCE_SIZE);
    new_chunk->size = size;
    new_chunk->free = 0;
    new_chunk->next = NULL;
    struct memory_chunk_t * temp_chunk =  memory_manager.first_memory_chunk;
    while(temp_chunk->next != NULL){
        temp_chunk = temp_chunk->next;
    }
    new_chunk->prev = temp_chunk;
    temp_chunk->next = new_chunk;
    int8_t * mem_mover = (int8_t *) new_chunk;
    mem_mover += sizeof(struct memory_chunk_t);
    for (int i = 0; i < FENCE_SIZE; ++i) {
        *mem_mover = 1;
        mem_mover++;
    }
    mem_pointer = memblock;
    for (int i = 0; i < (int)cur_chunk->size; ++i) {
        *mem_mover = *mem_pointer;
        mem_pointer++;
        mem_mover++;
    }
    mem_pointer = memblock;
    mem_mover = (int8_t *) new_chunk;
    mem_mover += sizeof(struct memory_chunk_t);
    mem_mover += size;
    mem_mover += FENCE_SIZE;
    for (int i = 0; i < FENCE_SIZE; ++i) {
        *mem_mover = 1;
        mem_mover++;
    }
    mem_mover = (int8_t *) new_chunk;
    mem_mover += sizeof(struct memory_chunk_t);
    mem_mover += FENCE_SIZE;
    heap_free(mem_pointer);
    return mem_mover;
}


void heap_free(void *address){
    if(address == NULL){
        return;
    }
    if(heap_validate() != 0){
        return;
    }
    int8_t * mem_pointer = address;
    mem_pointer -= sizeof(struct memory_chunk_t);
    mem_pointer -= FENCE_SIZE;
    struct memory_chunk_t * cur_chunk = (struct memory_chunk_t *) mem_pointer;
    struct memory_chunk_t * test_chunk = memory_manager.first_memory_chunk;
    int is_valid = 0;
    while (test_chunk != NULL){
        if(cur_chunk == test_chunk){
            is_valid = 1;
            break;
        }
        test_chunk = test_chunk->next;
    }
    if(is_valid == 0){
        return;
    }
    if(cur_chunk->next == NULL && cur_chunk->prev == NULL){
        memory_manager.first_memory_chunk = NULL;
        return;
    }
    if(cur_chunk->prev != NULL){
        if(cur_chunk->prev->free == 1){
            cur_chunk->prev->next = cur_chunk->next;
            if(cur_chunk->next != NULL){
                cur_chunk->next->prev = cur_chunk->prev;
            }
            cur_chunk = cur_chunk->prev;
        }
    }

    if(cur_chunk->next == NULL && cur_chunk->prev == NULL){
        memory_manager.first_memory_chunk = NULL;
        return;
    }
    if(cur_chunk->prev != NULL && cur_chunk->next == NULL){
        cur_chunk->prev->next = NULL;
        return;
    }

    if(cur_chunk->next != NULL){
        if(cur_chunk->next->free == 1){
            cur_chunk->next->next->prev = cur_chunk;
            cur_chunk->next = cur_chunk->next->next;
        }
    }
    if(cur_chunk->next == NULL && cur_chunk->prev == NULL){
        memory_manager.first_memory_chunk = NULL;
        return;
    }
    if(cur_chunk->prev != NULL && cur_chunk->next == NULL){
        cur_chunk->prev->next = NULL;
        return;
    }
    int8_t * next_chunk_pointer = (int8_t *) cur_chunk->next;
    mem_pointer = (int8_t *) cur_chunk;
    mem_pointer += sizeof(struct memory_chunk_t);
    int new_size = 0;
    while (mem_pointer != next_chunk_pointer){
        mem_pointer++;
        new_size++;
    }
    cur_chunk->free = 1;
    cur_chunk->size = new_size;
}

size_t   heap_get_largest_used_block_size(void){
    if(memory_manager.first_memory_chunk == NULL){
        return 0;
    }
    if(heap_validate() !=0){
        return 0;
    }
    struct memory_chunk_t * cur_chunk = memory_manager.first_memory_chunk;
    size_t max_size = 0;
    while (cur_chunk != NULL){

        if(cur_chunk->size > max_size && cur_chunk->free == 0){
            max_size = cur_chunk->size;
        }
        cur_chunk = cur_chunk->next;
    }
    return max_size;
}




