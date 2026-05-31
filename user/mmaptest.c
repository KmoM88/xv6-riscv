#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
  char *filename = "mmap_test_file";
  char *data = "Hello, Memory Mapped World!";
  int len = 4096;

  printf("--- MMAP TEST START ---\n\n");

  // 1. Create a file and populate it with initial data
  int fd = open(filename, O_CREATE | O_RDWR);
  if (fd < 0) {
    printf("open failed\n");
    exit(1);
  }
  write(fd, data, 28);
  close(fd);

  // 2. Open the file in read-write mode for mapping
  fd = open(filename, O_RDWR);
  if (fd < 0) {
    printf("re-open failed\n");
    exit(1);
  }

  // 3. Map the file sharing updates (flags=0x01: MAP_SHARED, prot=3: PROT_READ | PROT_WRITE)
  char *map = mmap(0, len, 1 | 2, 0x01, fd, 0);
  if (map == (char *)-1) {
    printf("mmap failed\n");
    exit(1);
  }

  printf("[mmap executed. Mapped address: %p]\n", map);
  printf("[Faulting-on-read: Reading content: \"%s\"]\n\n", map);

  // 4. Modify the contents directly in memory (setting the Dirty page flag)
  printf("[Modifying mapped memory directly...]\n");
  map[7] = 'W';
  map[8] = 'R';
  map[9] = 'I';
  map[10] = 'T';
  map[11] = 'E';
  printf("[Modification complete. Memory content: \"%s\"]\n\n", map);

  // 5. Unmap (forces writing the dirty page back to disk)
  printf("[Calling munmap to flush changes back to disk...]\n");
  if (munmap(map, len) < 0) {
    printf("munmap failed\n");
    exit(1);
  }
  close(fd);

  // 6. Re-open file and read back from disk to verify changes synced
  fd = open(filename, O_RDONLY);
  char buf[32];
  memset(buf, 0, sizeof(buf));
  read(fd, buf, 28);
  printf("[Verification: File content read from DISK: \"%s\"]\n\n", buf);
  close(fd);

  printf("--- MMAP TEST END ---\n");
  exit(0);
}
