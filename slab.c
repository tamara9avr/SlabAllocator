#include "slab.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

void kmem_init(void* space, int block_num)
{
	buddy = buddy_init(space, block_num);
	buffer_cache = buddy_alloc(sizeof(buffer_cache_t)*13+sizeof(kmem_cache_t));

	initialize_buffer_head();

	object_cache = (kmem_cache_t*)((size_t)buffer_cache+13*sizeof(buffer_cache_t));

	initialize_cache(object_cache,"Cache",sizeof(kmem_cache_t), NULL, NULL);
}


void initialize_cache(kmem_cache_t* cache, const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {
	cache->ctor = ctor;
	cache->dtor = dtor;
	cache->l1 = 0;
	cache->name = name;
	cache->size = size;
	cache->sizeChange = 1;
	cache->slabs[EMPTY] = NULL;
	cache->slabs[AVAILABLE] = NULL;
	cache->slabs[FULL] = NULL;
	cache->next = NULL;
}

void initialize_buffer_head() {
	for (uint8_t i=0; i < 13; i++) {
		buffer_cache[i].slabs[EMPTY] = NULL;
		buffer_cache[i].slabs[AVAILABLE] = NULL;
		buffer_cache[i].slabs[FULL] = NULL;
		buffer_cache[i].size = 1 << (i+5);
		buffer_cache[i].l1 = 0;
		buffer_cache[i].sizeChange = 0;
	}
}

size_t slab_size(size_t size) {
	size_t ret = 0;
	if (size > BLOCK_SIZE) {

		size = ceil((double)size / BLOCK_SIZE);

		size_t help = size;
		uint8_t cnt = -1;
		while (help > 0) {
			cnt = cnt + 1;
			help >>= 1;
		}
		if ((size - (1 << cnt)) != 0) {
			cnt += 1;
		}
		ret = cnt;
	}
	return 1<<ret;
}



kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*))
{
	kmem_cache_t* cachep = kmem_cache_alloc(object_cache);
	initialize_cache(cachep, name, size, ctor, dtor);
	return cachep;
}

int kmem_cache_shrink(kmem_cache_t* cachep)
{
	int cnt = 0;
	if (cachep->sizeChange == 0) {
		while (cachep->slabs[EMPTY]) {
			void* memptr = cachep->slabs[EMPTY];
			cachep->slabs[EMPTY] = cachep->slabs[EMPTY]->next;
			buddy_free(memptr, cachep->slabs[EMPTY]->slabSize / BLOCK_SIZE);
		}
	}
	cachep->sizeChange = 0;
	return cnt;
}

void* alloc_one_object(slab_head* slab)
{
	if (slab == NULL) return NULL;
	void* ret = NULL;
	while (slab) {
		for (uint8_t i = 0; i < slab->numOfSlots; i++) {
			if (slab->freeSlots[i] == 1) {
				ret = (void*)((size_t)slab->memmoryStart + i * slab->objectSize);
				slab->freeSlots[i] = 0;
				return ret;
			}
		}
		slab = slab->next;
	}
}

void* kmem_cache_alloc(kmem_cache_t* cachep)
{
	void* ret = NULL;
	if (cachep) {
		if (cachep->slabs[AVAILABLE]) {										//Ako ima u slobodnim
			ret = alloc_one_object(cachep->slabs[1]); 
			cachep->slabs[AVAILABLE]->numFreeSlots--;
			if (cachep->slabs[AVAILABLE]->numFreeSlots == 0)
				move_slab(cachep->slabs, cachep->slabs[AVAILABLE], FULL);

		}
		else if (cachep->slabs[EMPTY]) {								//Ako ima u praznim
			ret = alloc_one_object(cachep->slabs[EMPTY]);
			cachep->slabs[EMPTY]->numFreeSlots--;
			if (cachep->slabs[EMPTY]->numFreeSlots == 0)
				move_slab(cachep->slabs, cachep->slabs[EMPTY], FULL);
			else
				move_slab(cachep->slabs, cachep->slabs[EMPTY], AVAILABLE);
		}
		else {														//Ako treba da se napravi novi slab
			create_slab(cachep->slabs,cachep->size);
			ret = alloc_one_object(cachep->slabs[AVAILABLE]);
			cachep->slabs[AVAILABLE]->numFreeSlots--;
			cachep->sizeChange = 1;
			if (cachep->slabs[AVAILABLE]->numFreeSlots == 0)
				move_slab(cachep->slabs, cachep->slabs[AVAILABLE], FULL);

		}

		if (cachep->ctor) {
			cachep->ctor(ret);
		}
	}
	return ret;
}

void kmem_cache_free(kmem_cache_t* cachep, void* objp)
{
	slab_head* slab = find_slab(cachep->slabs, objp);
	size_t num = ((size_t)objp - (size_t)slab->memmoryStart) / slab->objectSize;
	slab->freeSlots[num] = 1;
	slab->numFreeSlots++;
	if (slab->numFreeSlots == slab->numOfSlots) {
		move_slab(cachep->slabs, slab, EMPTY);
	}
	else if (slab->type == FULL) {
		move_slab(cachep->slabs, slab, AVAILABLE);
	}
}



void move_slab(slab_head** slabs, slab_head* slab, enum SlabType t2) {
	slab_head* curr = slabs[slab->type], * prev = NULL;
	while (curr != slab) {
		prev = curr;
		curr = curr->next;
	}
	if (!prev)
		slabs[slab->type] = curr->next;
	else
		prev->next = curr->next;

	slab->type = t2;
	slab_head* curr2 = slabs[t2], *prev2 = NULL;
	while (curr2) {
		prev2 = curr2;
		curr = curr2->next;
	}
	if (prev2) {
		prev2->next = curr;
	}
	else {
		slabs[t2] = curr;
	}
}

void create_slab(slab_head** slabs, size_t objectSize) {

	size_t size = slab_size(objectSize + sizeof(slab_head)) * BLOCK_SIZE;
	slab_head* slab = buddy_alloc(size);										//dodaj proveru greske

	slab->slabSize = size;
	slab->objectSize = objectSize;
	slab->next = NULL;
	slab->type = AVAILABLE;
	slab->freeSlots = (void*)((size_t)slab + sizeof(slab_head));

	slab->numOfSlots = (size-sizeof(slab_head)) / objectSize;

	size = size - (slab->numOfSlots * sizeof(uint8_t));
	slab->numOfSlots = (size - sizeof(slab_head)) / objectSize;

	for (int i = 0; i < slab->numOfSlots; i++) {
		slab->freeSlots[i] = 1;
	}

	slab->numFreeSlots = slab->numOfSlots;
	slab->memmoryStart = (void*)((size_t)slab + sizeof(slab_head) + slab->numOfSlots * sizeof(uint8_t));

	slab_head* curr = slabs[AVAILABLE], * prev = NULL;
	while (curr) {
		prev = curr;
		curr = curr->next;
	}
	if (prev) prev->next = slab;
	else slabs[AVAILABLE] = slab;
}

void* alloc_one_buffer(slab_head* slab) {
	if (slab == NULL) return NULL;
	void* ret = NULL;
	while (slab) {
		for (uint8_t i = 0; i < slab->numOfSlots; i++) {
			if (slab->freeSlots[i] == 1) {
				ret = (void*)((size_t)slab->memmoryStart + i * slab->objectSize);
				slab->freeSlots[i] = 0;
				return ret;
			}
		}
		slab = slab->next;
	}
}

uint8_t closest_log1(size_t num) {
	uint8_t cnt = -1;
	while (num > 0) {
		cnt = cnt + 1;
		num >>= 1;
	}
	return cnt;
}

uint8_t block_size1(size_t par) {
	uint8_t ret = closest_log1(par);
	if ((par - (1 << ret)) != 0) {
		ret += 1;
	}
	return ret;
}

void* kmalloc(size_t size)
{
	size = block_size1(size) - 5;		//Dodaj proveru da li je velicina odgovarajuca
	void* ret = NULL;
	if (buffer_cache[size].slabs[AVAILABLE]) {										
		ret = alloc_one_buffer(buffer_cache[size].slabs[AVAILABLE]);
		buffer_cache[size].slabs[AVAILABLE]->numFreeSlots--;
		if (buffer_cache[size].slabs[AVAILABLE]->numFreeSlots == 0)
			move_slab(buffer_cache[size].slabs, buffer_cache[size].slabs[AVAILABLE], FULL);
	}
	else if (buffer_cache[size].slabs[EMPTY]) {
		ret = alloc_one_buffer(buffer_cache[size].slabs[EMPTY]);
		buffer_cache[size].slabs[EMPTY]->numFreeSlots--;
		if (buffer_cache[size].slabs[EMPTY]->numFreeSlots == 0)
			move_slab(buffer_cache[size].slabs, buffer_cache[size].slabs[EMPTY], FULL);
		else
			move_slab(buffer_cache[size].slabs, buffer_cache[size].slabs[EMPTY], AVAILABLE);
	}
	else {														
		create_slab(buffer_cache[size].slabs, buffer_cache[size].size);							//Dodaj proveru greske
		buffer_cache[size].sizeChange = 1;
		ret = alloc_one_buffer(buffer_cache[size].slabs[AVAILABLE]);
		buffer_cache[size].slabs[AVAILABLE]->numFreeSlots--;
		buffer_cache[size].sizeChange = 1;
		if (buffer_cache[size].slabs[AVAILABLE]->numFreeSlots == 0)
			move_slab(buffer_cache[size].slabs, buffer_cache[size].slabs[AVAILABLE], FULL);
	}
	return ret;
}

void kfree(const void* objp)
{
	buffer_cache_t* cachep = find_buffer_cache(objp);										//Dodaj proveru da li objp posoji
	slab_head* slab = find_slab(cachep->slabs, objp);										
	size_t num = ((size_t)objp - (size_t)slab->memmoryStart) / slab->objectSize;
	slab->freeSlots[num] = 1;
	slab->numFreeSlots++;
	if (slab->numFreeSlots == slab->numOfSlots) {
		move_slab(cachep->slabs, slab, EMPTY); 
	}
	else if (slab->type == FULL) {
		move_slab(cachep->slabs, slab, AVAILABLE);
	}
}



buffer_cache_t* find_buffer_cache(void* objp) {
	buffer_cache_t* ret = NULL;
	for (uint8_t i = 0; i < 13; i++) {
		for (uint8_t j = 0; j < 3; j++) {
			slab_head* slab = buffer_cache[i].slabs[j];
			while (slab) {
				if (slab->memmoryStart <= objp && ((size_t)slab->memmoryStart + slab->objectSize * slab->numOfSlots) >= objp) {
					return &buffer_cache[i];
				}
				slab = slab->next;
			}
		}
	}
}

slab_head* find_slab(slab_head** slabs, void* objp) {
	for (uint8_t j = 0; j < 3; j++) {
		slab_head* slab = slabs[j];
		while (slab) {
			if ( slab->memmoryStart <= objp && ((size_t)slab->memmoryStart + slab->objectSize * slab->numOfSlots) >= objp) {
				return slab;
			}
			slab = slab->next;
		}
	}
	return NULL;
}



void kmem_cache_destroy(kmem_cache_t* cachep)
{
}

void kmem_cache_info(kmem_cache_t* cachep)
{
	int numOfBlocks = 0, maxObjects= 0, freeObjects = 0;
	int numOfSlabs = 0;
	for (int i = 0; i < 3; i++) {
		slab_head* slab = cachep->slabs[i];
		while (slab) {
			numOfSlabs++;
			maxObjects += slab->numOfSlots;
			freeObjects = slab->numFreeSlots;
			numOfBlocks += (slab->numOfSlots - slab->numFreeSlots);
			slab = slab->next;
		}
	}
	printf_s("Cache info\nName: %d\nObject size: %d\nNum blocks: %d\nNumber slabs: %d\nNumber objects: %d\nPercentage: %f\n",
		cachep->name, cachep->size, numOfBlocks, numOfSlabs, maxObjects - freeObjects, ((double)maxObjects - freeObjects) / maxObjects);
}

int kmem_cache_error(kmem_cache_t* cachep)
{
	return 0;
}
