#include "buddy.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

void initialize_blocks() {
	void* start = head->memStart;
	int help = head->memSize / BLOCK_SIZE;							
	int id;
	while (help > 0) {													
		id = closest_log(help);
		block_head* block = (block_head*)start;
		block->next = NULL;
		head->entries[id].blocks = block;
		start = (block_head*)((size_t)start + BLOCK_SIZE * (1 << id));
		help -= (1 << id);
	}
}

void initialize_entries(int numOfBlocks) {
	void* start = head->memStart;
	int loss = 0;
	for (int i = 0; i < head->NumOfEntries; i++) {
		head->entries[i].blocks = NULL;
		head->entries[i].blocks = NULL;
		head->entries[i].FirstToMerge = start;
		int s = sizeof(int*) * numOfBlocks / (1 << (i + 1));
		start = (void*)((size_t)start + s);
		loss += s;
	}

	head->memSize -= loss;
	head->memStart = start;

	int num = head->memSize / BLOCK_SIZE;

	for (int i = 0; i < head->NumOfEntries; i++) {
		start = head->memStart;
		int j = 0; 
		
		while (start < (void*)((size_t)head->start + head->size)) {
			if (((size_t)start + BLOCK_SIZE * (1 << (i + 1))) <= ((size_t)head->start + head->size)) {
				head->entries[i].FirstToMerge[j] = start;
				j = j + 1;
			}
			start = (void*)((size_t)start + BLOCK_SIZE * (1 << (i + 1)));
		}
	}

	initialize_blocks();
}

buddy_head* buddy_init(void* memptr, int numOfBlocks)
{
	if (!memptr || !numOfBlocks) return NULL;

	size_t size = numOfBlocks*BLOCK_SIZE;

	if (size < sizeof(buddy_head)) return NULL;

	int numOfEntries = closest_log(numOfBlocks)+1;

	head = (buddy_head*)memptr;
	head->start = memptr;
	head->size = size;
	head->NumOfEntries = numOfEntries;
	head->memStart = (void*)((size_t)memptr + numOfEntries * sizeof(entry_head) + sizeof(buddy_head));

	if (head->memStart > (void*)((size_t)head->start + head->size)) return NULL;

	head->memSize = size - ((size_t)numOfEntries * sizeof(entry_head) + sizeof(buddy_head));
	head->entries = (entry_head*)((size_t)memptr + sizeof(buddy_head));
	InitializeCriticalSection(&head->lock);

	initialize_entries(numOfBlocks);

	return head;
}



void buddy_destroy()
{
	DeleteCriticalSection(&head->lock);
	free(head);
	head = NULL;
}

void* getBlock(int i) {
	void* ret = head->entries[i].blocks;
	if (ret) {
		head->entries[i].blocks = head->entries[i].blocks->next;
	}
	return ret;
}

void* split(void* memory, int min, int max) {
	while (max > min) {
		max--;
		insertBlock(memory, max);
		memory = (size_t)memory + (1 << max) * BLOCK_SIZE;
		if (max == min) {
			return memory;
		}
		else {
			split(memory, min, max);
		}
	}
}

void* findBlock(int i) {
	for (int j = i + 1; j < head->NumOfEntries; j++) {
		if (head->entries[j].blocks) {
			return split(getBlock(j),i, j);
		}
	}
	return NULL;
}

void* allocate(int i) {
	void* ret = NULL;
	ret = getBlock(i);
	if (!ret) {
		ret = findBlock(i);
	}
	return ret;
}

void* buddy_alloc(size_t memsize)
{
	void* ret = NULL;
	size_t help = ceil((double)memsize / BLOCK_SIZE);
	int id = block_size(help);

	if (id <= head->NumOfEntries)
	{
		EnterCriticalSection(&head->lock);
		ret = allocate(id);
		LeaveCriticalSection(&head->lock);
	}

	return ret;
}



void removeBlock(block_head* memptr, int i) {
	block_head* curr = head->entries[i].blocks, * prev = NULL;
	while (curr != memptr) {									
		prev = curr;
		curr = curr->next;
	}
	if (prev) {
		prev->next = curr->next;
	}
	else {
		head->entries[i].blocks = memptr->next;
	}
	memptr->next = NULL;
}

int* findPair(int* memptr, int i) {
	int numOfBlocks = head->memSize / BLOCK_SIZE;
	for (int k = 0; k < (numOfBlocks / (1 << (i + 1))); k++) {
		if ((head->entries[i].FirstToMerge[k] + BLOCK_SIZE * (1 << i)) == memptr) {
			return (int*)(head->entries[i].FirstToMerge[k]);
		}
		if (head->entries[i].FirstToMerge[k] == memptr) {
			return (int*)(head->entries[i].FirstToMerge[k] + BLOCK_SIZE * (1 << i));
		}
	}
	return NULL;
}

block_head* findAddr(int* memptr, int i) {
	block_head* curr = head->entries[i].blocks;
	while (curr && curr != (block_head*)memptr) {
		curr = curr->next;
	}
	return curr;
}

void insertBlock(void* memptr, int i) {
	int* pair = findPair(memptr, i);
	if (pair && findAddr(pair, i)) {
		removeBlock((block_head*)pair, i);
		if (pair > (int*)memptr) {
			insertBlock(memptr, i + 1);
		}
		else {
			insertBlock(pair, i + 1);
		}
	}
	else {
		if (!head->entries[i].blocks) {
			head->entries[i].blocks = (block_head*)memptr;
			head->entries[i].blocks->next = NULL;
		}
		else {
			block_head* prev = NULL, * curr = head->entries[i].blocks;
			while (curr) {
				prev = curr;
				curr = curr->next;
			}
			prev->next = (block_head*)memptr;
			prev->next->next = NULL;
		}
	}
}

void buddy_free(void* memptr, size_t memSize)
{
	EnterCriticalSection(&head->lock);
	if (memptr < head->start || memptr>(void*)((size_t)head->start + head->size)) return NULL;
	size_t help = ceil((double)memSize / BLOCK_SIZE);
	int numOfBlocks = block_size(help);
	insertBlock(memptr, numOfBlocks);
	LeaveCriticalSection(&head->lock);
}

