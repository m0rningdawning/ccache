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
void flush_cache(size_t l3_size);
void assess_cache(size_t size, size_t stride, size_t it, uint64_t *cycles,
                  uint64_t *nanos);

#endif /* CCACHE_H_ */
