
#include "global.h"


int closest_log(int num) {
	int cnt = -1;
	while (num > 0) {
		cnt = cnt + 1;
		num >>= 1;
	}
	return cnt;
}

int block_size(int par) {
	int ret = closest_log(par);
	if ((par - (1 << ret)) != 0) {
		ret += 1;
	}
	return ret;
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
	return 1 << ret;
}