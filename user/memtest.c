#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  uint64 m1 = freemem();
  printf("Free memory before allocation: %ld bytes\n", m1);

  // Allocate 1 page (4096 bytes)
  char *p = sbrk(4096);
  if (p == (char *)-1) {
    printf("sbrk allocation failed\n");
    exit(1);
  }

  uint64 m2 = freemem();
  printf("Free memory after allocating 1 page: %ld bytes\n", m2);

  if (m1 - m2 != 4096) {
    printf("Warning: Expected difference of 4096 bytes, got %ld\n", m1 - m2);
  } else {
    printf("Success: 1 page (4096 bytes) correctly allocated!\n");
  }

  // Deallocate page
  sbrk(-4096);
  uint64 m3 = freemem();
  printf("Free memory after deallocation: %ld bytes\n", m3);

  if (m3 != m1) {
    printf("Warning: Memory leak detected! Original: %ld, Reclaimed: %ld\n", m1, m3);
  } else {
    printf("Success: Memory successfully reclaimed!\n");
  }

  exit(0);
}
