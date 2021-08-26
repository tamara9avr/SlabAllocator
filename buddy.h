#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>
#include "global.h"

#define BLOCK_SIZE 4096

typedef struct Block_Head_Struct {
	struct Block_Head_Struct* next;
} block_head;

typedef struct Entry_Head_Stuct {
	block_head* blocks;
	int* FirstToMerge;
} entry_head;

typedef struct Buddy_Head_Struct {
	size_t size;
	void* start;
	size_t memSize;
	void* memStart;
	int NumOfEntries;
	entry_head* entries;
	CRITICAL_SECTION lock;
} buddy_head;

buddy_head* head;

void initialize_blocks();

void initialize_entries(int numOfBlocks);

buddy_head* buddy_init(void* memptr, int numOfBlocks);

void buddy_destroy();

void* getBlock(int i);

void* split(void* memptr, int min, int max);

void* findBlock(int i);

void* allocate(int i);

void* buddy_alloc(size_t memsize);

void removeBlock(block_head* memptr, int i);

int* findPair(int* memptr, int i);

block_head* findAddr(int* memptr, int i);

void insertBlock(void* memptr, int i);

void buddy_free(void* memptr, size_t memSize);

