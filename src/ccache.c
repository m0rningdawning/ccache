#include "ccache.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __x86_64__
static __inline uint64_t rdtsc(void) {
  unsigned hi, lo;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}
#else
#rdtsc is not available, using clock_gettime as fallback
static uint64_t rdtsc(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#endif

size_t get_cache_size(int level) {
#ifdef _SC_LEVEL1_DCACHE_SIZE
  int name = 0;
  switch (level) {
    case 1:
      name = _SC_LEVEL1_DCACHE_SIZE;
      break;
    case 2:
      name = _SC_LEVEL2_CACHE_SIZE;
      break;
    case 3:
      name = _SC_LEVEL3_CACHE_SIZE;
      break;
    default:
      return 0;
  }
  long val = sysconf(name);
  if (val > 0) return (size_t)val;
#endif
  return 0;
}

uint64_t assess_cache(size_t size, size_t stride, size_t it) {
  size_t elements = size / sizeof(size_t);
  size_t step = stride / sizeof(size_t);
  size_t *buffer = aligned_alloc(64, size);

  if (!buffer) {
    perror("aligned_alloc");
    exit(EXIT_FAILURE);
  }
  // pointer-chase list
  for (size_t i = 0; i < elements; i++) {
    buffer[i] = (i + step) % elements;
  }

  // warm-up
  size_t idx = 0;
  for (size_t i = 0; i < elements; i++) {
    idx = buffer[idx];
  }

  // timed run
  uint64_t start = rdtsc();
  idx = 0;
  for (size_t i = 0; i < it; i++) {
    idx = buffer[idx];
  }
  uint64_t end = rdtsc();

  free(buffer);
  return (end - start) / it;
}

int main(void) {
  size_t l1 = get_cache_size(1);
  size_t l2 = get_cache_size(2);
  size_t l3 = get_cache_size(3);

  if (!l1) l1 = 32 * 1024;
  if (!l2) l2 = 256 * 1024;
  if (!l3) l3 = 8 * 1024 * 1024;

  struct tests tests[] = {{"L1", l1, 64}, {"L2", l2, 64}, {"L3", l3, 64}};

  size_t it = 10000000;

  printf("Detected cache sizes: L1=%zu KB, L2=%zu KB, L3=%zu KB\n", l1 / 1024,
         l2 / 1024, l3 / 1024);
  printf("Measuring cache latencies (cycles/access) with %zu iterations each\n",
         it);

  for (int i = 0; i < 3; i++) {
    uint64_t lat = assess_cache(tests[i].size, tests[i].stride, it);
    printf("%-3s cache (%6zu KB): %4llu cycles\n", tests[i].name,
           tests[i].size / 1024, (unsigned long long)lat);
  }

  return 0;
}
