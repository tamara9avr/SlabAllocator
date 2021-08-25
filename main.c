#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "slab.h"
#include "test.h"

#define BLOCK_NUMBER (1000)
#define THREAD_NUM (1)
#define ITERATIONS (250)

#define shared_size (7)


void construct(void *data) {
	static int i = 1;
	printf_s("%d Shared object constructed.\n", i++);
	memset(data, MASK, shared_size);
}

int check(void *data, size_t size) {
	int ret = 1;
	for (int i = 0; i < size; i++) {
		if (((unsigned char *)data)[i] != MASK) {
			ret = 0;
		}
	}

	return ret;
}

struct objects_s {
	kmem_cache_t *cache;
	void *data;
};

void work(void* pdata) {
	struct data_s data = *(struct data_s*) pdata;
	char buffer[1024];
	int size = 0;
	sprintf_s(buffer, 1024, "thread cache %d", data.id);
	kmem_cache_t *cache = kmem_cache_create(buffer, data.id, 0, 0);

	struct objects_s *objs = (struct objects_s*)(kmalloc(sizeof(struct objects_s) * data.iterations));

	for (int i = 0; i < data.iterations; i++) {
		if (i % 100 == 0) {
			objs[size].data = kmem_cache_alloc(data.shared);
			objs[size].cache = data.shared;
			assert(check(objs[size].data, shared_size));
		}
		else {
			objs[size].data = kmem_cache_alloc(cache);
			objs[size].cache = cache;
			memset(objs[size].data, MASK, data.id);
		}
		size++;
	}

	kmem_cache_info(cache);
	kmem_cache_info(data.shared);

	for (int i = 0; i < size; i++) {
		assert(check(objs[i].data, (cache == objs[i].cache) ? data.id : shared_size));
		kmem_cache_free(objs[i].cache, objs[i].data);
	}

	kfree(objs);
	kmem_cache_destroy(cache);
}


int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);
	kmem_init(space, BLOCK_NUMBER);
	kmem_cache_t *shared = kmem_cache_create("shared object", shared_size, construct, NULL);

	struct data_s data;
	data.shared = shared;
	data.iterations = ITERATIONS;
	run_threads(work, &data, THREAD_NUM);

	kmem_cache_destroy(shared);
	free(space);
	//void* memptr = malloc(BLOCK_SIZE * 17);

	/*buddy_init(memptr, 17);
	void * ptr1 = buddy_alloc(2048);
	void* ptr2 = buddy_alloc(BLOCK_SIZE);
	void* ptr3 = buddy_alloc(2 * BLOCK_SIZE);
	void* ptr4 = buddy_alloc(35);

	int pp = ptr4;
	int pp1 = ((size_t) ptr4 + BLOCK_SIZE);

	buddy_free(ptr1, 2048);
	buddy_free(ptr3, 8192);
	buddy_free(ptr4, 35);
	buddy_free(ptr2, 4096);

	buddy_destroy();*/

	//kmem_init(memptr, 17);
	//kmem_cache_t* c1 = kmem_cache_create("Cache 1", 10, const1, NULL);
	//kmem_cache_t* c2 = kmem_cache_create("Cache 2", 7, const2, NULL);


	//for (int i = 0; i < 1000; i++) {
	//	if (i % 50 == 0) {
	//		void* data = kmem_cache_alloc(c1);
	//	}
	//	else {
	//		void* data = kmem_cache_alloc(c2);
	//	}
	//}

	//kmem_cache_info(c1);
	//kmem_cache_info(c2);

	return 0;
}
