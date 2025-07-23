#include "ccache.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __x86_64__
static inline uint64_t rdtsc(void) {
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

void flush_cache(size_t l3_size) {
  size_t flush_size = l3_size * 2;
  size_t elements = flush_size / sizeof(size_t);
  volatile size_t *flush_buf = aligned_alloc(64, flush_size);
  size_t sum = 0;

  if (!flush_buf) {
    perror("aligned_alloc");
    exit(EXIT_FAILURE);
  }

  // Init buffer with a pointer-chasing pattern
  for (size_t i = 0; i < elements; i++) {
    ((size_t *)flush_buf)[i] = (i + 97) % elements;
  }

  // Pointer-chasing access to defeat prefetchers
  size_t idx = 0;
  for (size_t i = 0; i < elements; i++) {
    idx = flush_buf[idx];
    sum += idx;
  }

  // Prevent optimizing away
  if (sum == 0xdeadbeef) puts("Impossible");
  free((void *)flush_buf);
}

static inline uint64_t timespec_diff_ns(const struct timespec *start,
                                        const struct timespec *end) {
  return (end->tv_sec - start->tv_sec) * 1000000000ULL +
         (end->tv_nsec - start->tv_nsec);
}

void assess_cache(size_t size, size_t stride, size_t it, uint64_t *cycles,
                  uint64_t *nanos) {
  size_t elements = size / sizeof(size_t);
  size_t step = stride / sizeof(size_t);
  size_t *buffer = aligned_alloc(64, size);

  if (!buffer) {
    perror("aligned_alloc");
    exit(EXIT_FAILURE);
  }
  // Pointer-chase list
  for (size_t i = 0; i < elements; i++) {
    buffer[i] = (i + step) % elements;
  }

  // Warm-up
  size_t idx = 0;
  for (size_t i = 0; i < elements; i++) {
    idx = buffer[idx];
  }

  flush_cache(get_cache_size(3) ? get_cache_size(3) : 8 * 1024 * 1024);

  // Cycles and ns
  uint64_t start_cycles = rdtsc();
  struct timespec t_start, t_end;
  clock_gettime(CLOCK_MONOTONIC, &t_start);

  idx = 0;
  for (size_t i = 0; i < it; i++) {
    idx = buffer[idx];
  }

  clock_gettime(CLOCK_MONOTONIC, &t_end);
  uint64_t end_cycles = rdtsc();

  free(buffer);

  *cycles = (end_cycles - start_cycles) / it;
  *nanos = timespec_diff_ns(&t_start, &t_end) / it;
}

int main(void) {
  size_t l1 = get_cache_size(1);
  size_t l2 = get_cache_size(2);
  size_t l3 = get_cache_size(3);
  size_t it = 10000000;

  if (!l1) l1 = 32 * 1024;
  if (!l2) l2 = 256 * 1024;
  if (!l3) l3 = 8 * 1024 * 1024;

  struct tests tests[] = {{"L1", l1, 64}, {"L2", l2, 64}, {"L3", l3, 64}};

  printf("Detected cache sizes: L1=%zu KB, L2=%zu KB, L3=%zu KB\n", l1 / 1024,
         l2 / 1024, l3 / 1024);
  printf(
      "Measuring cache latencies (cycles/access and ns/access) with %zu "
      "iterations each\n",
      it);

  for (int i = 0; i < 3; i++) {
    uint64_t lat_cycles, lat_ns;
    assess_cache(tests[i].size, tests[i].stride, it, &lat_cycles, &lat_ns);
    printf("%-3s cache (%6zu KB): %4llu cycles, %4llu ns\n", tests[i].name,
           tests[i].size / 1024, (unsigned long long)lat_cycles,
           (unsigned long long)lat_ns);
  }

  return 0;
}
