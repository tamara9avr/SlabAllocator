#pragma once

#include <stdlib.h>
#include "buddy.h"

typedef enum SLAB_TYPE {
	EMPTY = 0,
	AVAILABLE = 1,
	FULL = 2
} SlabType;

typedef struct slab_head_struct {
	struct slab_head_struct* next;
	size_t numFreeSlots;
	void* memmoryStart;
	size_t slabSize;
	size_t numOfSlots;
	size_t objectSize;
	SlabType type;
	uint8_t* freeSlots;
} slab_head;

typedef struct kmem_cache_s {
	CRITICAL_SECTION lock;
	char* error;
	char* name;
	size_t size;
	void (*ctor)(void*);
	void (*dtor)(void*);
	boolean sizeChange;
	slab_head* slabs[3];
	size_t l1;
} kmem_cache_t;

typedef struct buffer_cache_s {
	CRITICAL_SECTION lock;
	char* error;
	size_t size;
	boolean sizeChange;
	slab_head* slabs[3];
	size_t l1;
} buffer_cache_t;

#define BLOCK_SIZE (4096)
#define CACHE_L1_LINE_SIZE (64)
#define MAX_BUFFER_SIZE 17
#define MIN_BUFFER_SIZE 5

buddy_head* buddy;
buffer_cache_t* buffer_cache;
kmem_cache_t* object_cache;

void kmem_init(void* space, int block_num);

void initialize_cache(kmem_cache_t* cache, const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*));

void initialize_buffer_head();

kmem_cache_t * kmem_cache_create(const char* name, size_t size,void (*ctor)(void*),void (*dtor)(void*)); // Allocate cache

int kmem_cache_shrink(kmem_cache_t * cachep); // Shrink cache

void* kmem_cache_alloc(kmem_cache_t * cachep); // Allocate one object from cache

void create_slab(slab_head** slabs, size_t objectSize, size_t*l1);

void* alloc_one_object(slab_head* slab);

void kmem_cache_free(kmem_cache_t * cachep, void* objp); // Deallocate one object from cache

void move_slab(slab_head** slabs, slab_head* slab, SlabType t2);

int buffer_cache_shrink(buffer_cache_t* cachep);

void* kmalloc(size_t size); // Alloacate one small memory buffer

void kfree(const void* objp); // Deallocate one small memory buffer

buffer_cache_t* find_buffer_cache(void* objp);

slab_head* find_slab(slab_head** slabs, void* objp);

void kmem_cache_destroy(kmem_cache_t* cachep); // Deallocate cache

void kmem_cache_info(kmem_cache_t* cachep); // Print cache info

int kmem_cache_error(kmem_cache_t* cachep); // Print error message
