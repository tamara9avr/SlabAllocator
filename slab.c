#include "slab.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

void kmem_init(void* space, int block_num)
{
	buddy = buddy_init(space, block_num);

	if (!buddy) {
		printf_s("Not enough memmory to initialize buddy\n");
		return;
	}

	buffer_cache = buddy_alloc(sizeof(buffer_cache_t)*(MAX_BUFFER_SIZE-MIN_BUFFER_SIZE+1)+sizeof(kmem_cache_t));

	if (!buffer_cache) {
		printf_s("Not enough memmory to initialize cache\n");
		return;
	}

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
	cache->error = NULL;
	InitializeCriticalSection(&cache->lock);
}

void initialize_buffer_head() {
	for (uint8_t i=0; i < (MAX_BUFFER_SIZE - MIN_BUFFER_SIZE + 1); i++) {
		buffer_cache[i].slabs[EMPTY] = NULL;
		buffer_cache[i].slabs[AVAILABLE] = NULL;
		buffer_cache[i].slabs[FULL] = NULL;
		buffer_cache[i].size = 1 << (i+5);
		buffer_cache[i].l1 = 0;
		buffer_cache[i].sizeChange = 0;
		buffer_cache[i].error = NULL;
		InitializeCriticalSection(&buffer_cache[i].lock);
	}
}



kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*))
{
	kmem_cache_t* cachep = kmem_cache_alloc(object_cache);

	if (!cachep) {
		printf_s("Cache create fail\n");
		return NULL;
	}

	initialize_cache(cachep, name, size, ctor, dtor);
	return cachep;
}

int kmem_cache_shrink(kmem_cache_t* cachep)
{
	if (!cachep) {
		return 0;
	}
	EnterCriticalSection(&object_cache->lock);
	int cnt = 0;
	if (cachep->sizeChange == 0) {
		while (cachep->slabs[EMPTY]) {
			void* memptr = cachep->slabs[EMPTY];
			slab_head* next = cachep->slabs[EMPTY]->next;
			buddy_free(memptr, cachep->slabs[EMPTY]->slabSize);
			cachep->slabs[EMPTY] = next;
			cnt++;
		}
	//	cachep->error = "Shrink done";
	//	kmem_cache_error(cachep);
	}
	else {
	//	cachep->error = "Shrink not executed";
	//	kmem_cache_error(cachep);
	}
	cachep->sizeChange = 0;
	LeaveCriticalSection(&object_cache->lock);
	return cnt;
}

void* alloc_one_object(slab_head* slab)
{
	if (!slab) return NULL;
	void* ret = NULL;
	for (int i = 0; i < slab->numOfSlots; i++) {
		if (slab->freeSlots[i] == 1) {
			ret = (void*)((size_t)slab->memmoryStart + i * slab->objectSize);
			slab->freeSlots[i] = 0;
			slab->numFreeSlots--;
			return ret;
		}
	}
	return ret;
}



void* kmem_cache_alloc(kmem_cache_t* cachep)
{
	if (!cachep) return NULL;
	EnterCriticalSection(&cachep->lock);
	void* ret = NULL;
	if (cachep ) {
		if (cachep->slabs[AVAILABLE]) {									
			ret = alloc_one_object(cachep->slabs[1]); 

			if (!ret) {
				cachep->error = "Allocation failed";
				LeaveCriticalSection(&cachep->lock);
				kmem_cache_error(cachep);
				return NULL;
			}

			if (!cachep->slabs[AVAILABLE]->numFreeSlots)
				move_slab(cachep->slabs, cachep->slabs[AVAILABLE], FULL);
		}
		else if (cachep->slabs[EMPTY]) {						
			ret = alloc_one_object(cachep->slabs[EMPTY]);

			if (!ret) {
				cachep->error = "Allocation failed";
				LeaveCriticalSection(&cachep->lock);
				kmem_cache_error(cachep);
				return NULL;
			}

			if (!cachep->slabs[EMPTY]->numFreeSlots)
				move_slab(cachep->slabs, cachep->slabs[EMPTY], FULL);
			else
				move_slab(cachep->slabs, cachep->slabs[EMPTY], AVAILABLE);
		}
		else {												
			create_slab(cachep->slabs,cachep->size, &cachep->l1);

			if (!cachep->slabs[AVAILABLE]) {
				cachep->error = "Fail creating slab";
				LeaveCriticalSection(&cachep->lock);
				kmem_cache_error(cachep);
				return NULL;
			}

			ret = alloc_one_object(cachep->slabs[AVAILABLE]);

			if (!ret) {
				cachep->error = "Allocation failed";
				LeaveCriticalSection(&cachep->lock);
				kmem_cache_error(cachep);
				return NULL;
			}

			cachep->sizeChange = 1;
			if (!cachep->slabs[AVAILABLE]->numFreeSlots)
				move_slab(cachep->slabs, cachep->slabs[AVAILABLE], FULL);

		}

		if (cachep->ctor) {
			cachep->ctor(ret);
		}

	}
	LeaveCriticalSection(&cachep->lock);
	return ret;
}

void kmem_cache_free(kmem_cache_t* cachep, void* objp)
{
	EnterCriticalSection(&cachep->lock);
	slab_head* slab = find_slab(cachep->slabs, objp);
	if (!slab) {
		cachep->error = "Object is not in cache";
		kmem_cache_error(cachep);
		LeaveCriticalSection(&cachep->lock);
		return NULL;
	}
	size_t num = ((size_t)objp - (size_t)slab->memmoryStart) / slab->objectSize;
	slab->freeSlots[num] = 1;
	slab->numFreeSlots++;
	if (slab->numFreeSlots == slab->numOfSlots) {
		move_slab(cachep->slabs, slab, EMPTY);
	}
	else if (slab->type == FULL) {
		move_slab(cachep->slabs, slab, AVAILABLE);
	}
	LeaveCriticalSection(&cachep->lock);
}



void move_slab(slab_head** slabs, slab_head* slab, SlabType t2) {
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
	slab->next = NULL;
	slab_head* curr2 = slabs[t2], *prev2 = NULL;
	while (curr2) {
		prev2 = curr2;
		curr2 = curr2->next;
	}
	if (prev2) {
		prev2->next = slab;
	}
	else {
		slabs[t2] = slab;
	}
}

int buffer_cache_shrink(buffer_cache_t* cachep)
{
	if (!cachep) {
		return 0;
	}
	EnterCriticalSection(&object_cache->lock);
	int cnt = 0;
	if (cachep->sizeChange == 0) {
		while (cachep->slabs[EMPTY]) {
			void* memptr = cachep->slabs[EMPTY];
			slab_head* next = cachep->slabs[EMPTY]->next;
			buddy_free(memptr, cachep->slabs[EMPTY]->slabSize);
			cachep->slabs[EMPTY] = next;
			cnt++;
		}
	//	printf_s("Shrink done\n");
	}
	else {
	//	printf_s("Shrink not executed\n");
	}
	cachep->sizeChange = 0;
	LeaveCriticalSection(&object_cache->lock);
	return cnt;
}

void create_slab(slab_head** slabs, size_t objectSize, size_t* l1) {

	size_t size = slab_size(objectSize + sizeof(slab_head)) * BLOCK_SIZE;
	slab_head* slab = buddy_alloc(size);									

	if (!slab) return;

	slab->slabSize = size;
	slab->objectSize = objectSize;
	slab->next = NULL;
	slab->type = AVAILABLE;

	size_t num = (size-sizeof(slab_head)) / objectSize;

	size_t si = sizeof(uint8_t) * num;
	slab->numOfSlots = (size - sizeof(slab_head) - si) / objectSize;

	if (slab->numOfSlots == 0) {
		num = (size - sizeof(slab_head)) / (objectSize*2);
		si = sizeof(uint8_t) * num;
		slab->numOfSlots = (size - sizeof(slab_head) - si) / objectSize;
	}

	size_t freeSpace = size - sizeof(slab_head) - si- (slab->numOfSlots * objectSize);
	size_t offset = *l1 + CACHE_L1_LINE_SIZE;

	if (offset > freeSpace) offset = 0;

	*l1 = offset;

	slab->memmoryStart = (void*)((size_t)slab + sizeof(slab_head)+sizeof(uint8_t)*num);


	slab->freeSlots = (void*)((size_t)slab + sizeof(slab_head));

	for (int i = 0; i < slab->numOfSlots; i++) {
		slab->freeSlots[i] = 1;
	}

	slab->numFreeSlots = slab->numOfSlots;

	slab_head* curr = slabs[AVAILABLE], * prev = NULL;
	while (curr) {
		prev = curr;
		curr = curr->next;
	}
	if (prev) prev->next = slab;
	else slabs[AVAILABLE] = slab;
}


void* kmalloc(size_t size)
{
	size = block_size(size);
	if ((size < MIN_BUFFER_SIZE) && (size > MAX_BUFFER_SIZE)) {
		printf_s("Bad buffer size");
		return NULL;
	}


	int id = size - MIN_BUFFER_SIZE;
	EnterCriticalSection(&buffer_cache[id].lock);

	void* ret = NULL;

	if (buffer_cache[id].slabs[AVAILABLE]) {										
		ret = alloc_one_object(buffer_cache[id].slabs[AVAILABLE]);
		if (!ret) {
			printf_s("Buffer allocation failed\n");
			LeaveCriticalSection(&buffer_cache[id].lock);
			return NULL;
		}
		if (!buffer_cache[id].slabs[AVAILABLE]->numFreeSlots)
			move_slab(buffer_cache[id].slabs, buffer_cache[id].slabs[AVAILABLE], FULL);
	}

	else if (buffer_cache[id].slabs[EMPTY]) {
		ret = alloc_one_object(buffer_cache[id].slabs[EMPTY]); 
		if (!ret) {
			printf_s("Buffer allocation failed\n");
			LeaveCriticalSection(&buffer_cache[id].lock);
			return NULL;
		}
		if (!buffer_cache[id].slabs[EMPTY]->numFreeSlots)
			move_slab(buffer_cache[id].slabs, buffer_cache[id].slabs[EMPTY], FULL);
		else
			move_slab(buffer_cache[id].slabs, buffer_cache[id].slabs[EMPTY], AVAILABLE);
	}

	else {														
		create_slab(buffer_cache[id].slabs, buffer_cache[id].size, &buffer_cache[id].l1);	

		if (!buffer_cache[id].slabs[AVAILABLE]) {
			printf_s("Failed creating slab, not enough memmory\n");
			LeaveCriticalSection(&buffer_cache[id].lock);
			return NULL;
		}

		ret = alloc_one_object(buffer_cache[id].slabs[AVAILABLE]);
		
		if (!ret) {
			printf_s("Buffer allocation failed\n");
			LeaveCriticalSection(&buffer_cache[id].lock);
			return NULL;
		}

		buffer_cache[id].sizeChange = 1;
		if (!buffer_cache[id].slabs[AVAILABLE]->numFreeSlots)
			move_slab(buffer_cache[id].slabs, buffer_cache[id].slabs[AVAILABLE], FULL);
	}

	LeaveCriticalSection(&buffer_cache[id].lock);
	return ret;
}

void kfree(const void* objp)
{
	buffer_cache_t* cachep = find_buffer_cache(objp);		

	if (!cachep) {
		printf_s("Object is not in cache\n");
		return NULL;
	}

	EnterCriticalSection(&cachep->lock);


	slab_head* slab = find_slab(cachep->slabs, objp);	

	if (!slab) {
		printf_s("Object not in cache\n");
		LeaveCriticalSection(&cachep->lock);
		return NULL;
	}

	size_t num = ((size_t)objp - (size_t)slab->memmoryStart) / slab->objectSize;
	slab->freeSlots[num] = 1;
	if (slab->numFreeSlots == slab->numOfSlots) {
		move_slab(cachep->slabs, slab, EMPTY); 
		buffer_cache_shrink(cachep);
	}
	else if (slab->type == FULL) {
		move_slab(cachep->slabs, slab, AVAILABLE);
	}

	LeaveCriticalSection(&cachep->lock);
}



buffer_cache_t* find_buffer_cache(void* objp) {
	for (uint8_t i = 0; i < (MAX_BUFFER_SIZE - MIN_BUFFER_SIZE + 1); i++) {
		for (uint8_t j = 0; j < 3; j++) {
			slab_head* slab = buffer_cache[i].slabs[j];
			while (slab) {
				if (slab->memmoryStart <= objp && (void*)((size_t)slab->memmoryStart + slab->objectSize * slab->numOfSlots) >= objp) {
					return &buffer_cache[i];
				}
				slab = slab->next;
			}
		}
	}
	return NULL;
}

slab_head* find_slab(slab_head** slabs, void* objp) {
	for (uint8_t j = 0; j < 3; j++) {
		slab_head* slab = slabs[j];
		while (slab) {
			if ( slab->memmoryStart <= objp && (void*)((size_t)slab->memmoryStart + slab->objectSize * slab->numOfSlots) >= objp) {
				return slab;
			}
			slab = slab->next;
		}
	}
	return NULL;
}



void kmem_cache_destroy(kmem_cache_t* cachep)
{
	EnterCriticalSection(&cachep->lock);
	for (int i = 1; i < 3; i++) {
		slab_head* slab = cachep->slabs[i];
		while (slab) {
			for (int j = 0; j < slab->numOfSlots; j++) {
				if (slab->freeSlots[j] == 0) {
					slab->freeSlots[j] = 1;
					slab->numFreeSlots++;
				}
			}
			move_slab(cachep->slabs, slab, EMPTY);
			slab = slab->next;
		}
	}
	cachep->sizeChange = 0;
	kmem_cache_shrink(cachep);

	LeaveCriticalSection(&cachep->lock);
	DeleteCriticalSection(&cachep->lock);

	kmem_cache_free(object_cache, cachep);
	cachep = NULL;
}

void kmem_cache_info(kmem_cache_t* cachep)
{
	EnterCriticalSection(&object_cache->lock);
	int numOfBlocks = 0, maxObjects= 0, freeObjects = 0;
	int numOfSlabs = 0;
	for (int i = 0; i < 3; i++) {
		slab_head* slab = cachep->slabs[i];
		while (slab) {
			numOfSlabs++;
			maxObjects += slab->numOfSlots;
			freeObjects += slab->numFreeSlots;
			numOfBlocks += (slab->slabSize / BLOCK_SIZE);
			slab = slab->next;
		}
	}
	LeaveCriticalSection(&object_cache->lock);
	printf_s("Cache info\nName: %s\nObject size: %d\nNum blocks: %d\nNumber slabs: %d\nNumber objects: %d\nPercentage: %f \n",
		cachep->name, cachep->size, numOfBlocks, numOfSlabs, maxObjects - freeObjects, ((double)maxObjects - freeObjects) / maxObjects * 100);
}

int kmem_cache_error(kmem_cache_t* cachep)
{
	printf_s("Cache msg \nName: %s\nMessage: %s\n", cachep->name, cachep->error);
}
