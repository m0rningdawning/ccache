#ifndef CCACHE_H_
#define CCACHE_H_

#include <stddef.h>
#include <stdint.h>

struct tests {
  const char *name;
  size_t size;
  size_t stride;
};

size_t get_cache_size(int level);
uint64_t assess_cache(size_t size, size_t stride, size_t it);

#endif /* CCACHE_H_ */
