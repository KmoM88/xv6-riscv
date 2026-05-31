#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  printf("--- COW TEST START ---\n\n");

  // 1. Allocate a page dynamically using sbrk
  char *addr = sbrk(4096);
  if (addr == (char *)-1) {
    printf("sbrk failed\n");
    exit(1);
  }

  // 2. Populate page with initial data
  addr[0] = 'X';
  addr[1] = 'Y';
  addr[2] = 'Z';
  addr[3] = '\0';

  printf("[Parent: Initial page mapped at %p. Value: \"%s\"]\n", addr, addr);

  // 3. Fork a child process
  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child process: reads shared page (proves read sharing)
    printf("[Child: Reading shared parent page... Value: \"%s\"]\n", addr);
    if (addr[0] != 'X' || addr[1] != 'Y' || addr[2] != 'Z') {
      printf("Error: Child read wrong values!\n");
      exit(1);
    }

    // Writes to shared page (proves store page fault triggers copy-on-write duplication)
    printf("[Child: Writing to shared page to trigger COW fault...]\n");
    addr[0] = 'A';
    addr[1] = 'B';
    addr[2] = 'C';
    printf("[Child: COW page copied and updated. Value: \"%s\"]\n", addr);

    if (addr[0] != 'A' || addr[1] != 'B' || addr[2] != 'C') {
      printf("Error: Child write did not update memory!\n");
      exit(1);
    }
    exit(0);
  } else {
    // Parent process: waits for child to exit and checks original page content
    wait(0);
    printf("[Parent: Child exited. Checking original shared page... Value: \"%s\"]\n", addr);

    if (addr[0] != 'X' || addr[1] != 'Y' || addr[2] != 'Z') {
      printf("Error: Parent memory was corrupted by child write! COW failed.\n");
      exit(1);
    }
    printf("\n[COW verification: Parent page remained untouched! COW worked!]\n\n");
  }

  printf("--- COW TEST END ---\n");
  exit(0);
}
