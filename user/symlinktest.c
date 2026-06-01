#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define FAIL(msg) \
  do { \
    printf("FAIL: %s at line %d\n", msg, __LINE__); \
    exit(1); \
  } while (0)

#define OK(msg) printf("OK: %s\n", msg)

void
test_basic(void)
{
  int fd;
  char buf[32];

  printf("--- RUNNING BASIC SYMLINK TEST ---\n");

  // Create target file and write data to it
  if ((fd = open("symtarget", O_CREATE | O_WRONLY)) < 0) {
    FAIL("create symtarget");
  }
  if (write(fd, "hello symlinks!", 15) != 15) {
    FAIL("write to symtarget");
  }
  close(fd);

  // Create symbolic link pointing to "symtarget"
  if (symlink("symtarget", "symlink1") < 0) {
    FAIL("create symlink1");
  }

  // Open symlink (should resolve to "symtarget" and read data)
  if ((fd = open("symlink1", O_RDONLY)) < 0) {
    FAIL("open symlink1");
  }
  memset(buf, 0, sizeof(buf));
  if (read(fd, buf, sizeof(buf)) != 15) {
    FAIL("read from symlink1");
  }
  if (strcmp(buf, "hello symlinks!") != 0) {
    FAIL("data read mismatch");
  }
  close(fd);

  // Clean up
  unlink("symlink1");
  unlink("symtarget");

  OK("basic symlink test passed");
}

void
test_recursive(void)
{
  int fd;
  char buf[32];

  printf("--- RUNNING RECURSIVE SYMLINK TEST ---\n");

  // Create target file
  if ((fd = open("target_rec", O_CREATE | O_WRONLY)) < 0) {
    FAIL("create target_rec");
  }
  if (write(fd, "deep recursion", 14) != 14) {
    FAIL("write to target_rec");
  }
  close(fd);

  // Create recursive symlinks: link3 -> link2 -> link1 -> target_rec
  if (symlink("target_rec", "link1") < 0) {
    FAIL("create link1");
  }
  if (symlink("link1", "link2") < 0) {
    FAIL("create link2");
  }
  if (symlink("link2", "link3") < 0) {
    FAIL("create link3");
  }

  // Open link3
  if ((fd = open("link3", O_RDONLY)) < 0) {
    FAIL("open link3");
  }
  memset(buf, 0, sizeof(buf));
  if (read(fd, buf, sizeof(buf)) != 14) {
    FAIL("read from link3");
  }
  if (strcmp(buf, "deep recursion") != 0) {
    FAIL("data read mismatch on recursive link");
  }
  close(fd);

  // Clean up
  unlink("link3");
  unlink("link2");
  unlink("link1");
  unlink("target_rec");

  OK("recursive symlink test passed");
}

void
test_nofollow(void)
{
  int fd;

  printf("--- RUNNING O_NOFOLLOW TEST ---\n");

  // Create target and symlink
  if ((fd = open("target_nf", O_CREATE | O_WRONLY)) < 0) {
    FAIL("create target_nf");
  }
  close(fd);

  if (symlink("target_nf", "link_nf") < 0) {
    FAIL("create link_nf");
  }

  // Open with O_NOFOLLOW should return the link itself instead of the target
  if ((fd = open("link_nf", O_RDONLY | O_NOFOLLOW)) < 0) {
    FAIL("open link_nf with O_NOFOLLOW");
  }
  close(fd);

  // Clean up
  unlink("link_nf");
  unlink("target_nf");

  OK("O_NOFOLLOW test passed");
}

void
test_circular(void)
{
  int fd;

  printf("--- RUNNING CIRCULAR LOOP DETECTION TEST ---\n");

  // Create circular symlinks: link_a -> link_b -> link_a
  if (symlink("link_b", "link_a") < 0) {
    FAIL("create link_a");
  }
  if (symlink("link_a", "link_b") < 0) {
    FAIL("create link_b");
  }

  // Open should fail and return -1 due to infinite recursion
  if ((fd = open("link_a", O_RDONLY)) >= 0) {
    close(fd);
    FAIL("circular symlink loop not detected!");
  }

  // Clean up
  unlink("link_a");
  unlink("link_b");

  OK("circular loop detection test passed");
}

int
main(void)
{
  printf("--- STARTING SYMLINK VERIFICATION SUITE ---\n");
  test_basic();
  test_recursive();
  test_nofollow();
  test_circular();
  printf("ALL SYMLINK TESTS PASSED SUCCESSFULLY!\n");
  exit(0);
}
