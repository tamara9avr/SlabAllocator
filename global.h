#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define BLOCK_SIZE 4096


int closest_log(int num);

int block_size(int par);

size_t slab_size(size_t size);