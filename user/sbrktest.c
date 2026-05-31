#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("--- LAZY ALLOCATION TEST START ---\n\n");

  printf("==========================================\n");
  printf("1. Page Table BEFORE sbrk(1):\n");
  printf("==========================================\n");
  pgprint();

  // Request 1 byte. Under lazy sbrk(), size grows but no allocation occurs.
  char *addr = sbrklazy(1);
  if (addr == (char *)-1) {
    printf("sbrklazy failed!\n");
    exit(1);
  }
  printf("\n[sbrklazy(1) executed. Heap boundary address returned: %p]\n\n", addr);


  printf("==========================================\n");
  printf("2. Page Table AFTER sbrk(1) (Unaccessed Lazy State):\n");
  printf("==========================================\n");
  pgprint();

  // Write a byte to trigger store page fault and force vmfault() mapping.
  printf("\n[Writing to lazy address %p...]\n", addr);
  *addr = 'A';
  printf("[Write complete. Physical page should now be mapped!]\n\n");

  printf("==========================================\n");
  printf("3. Page Table AFTER Write Access (Allocated State):\n");
  printf("==========================================\n");
  pgprint();

  printf("\n--- LAZY ALLOCATION TEST END ---\n");
  exit(0);
}
