#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define TEST_SIZE 10000
#define CHUNK_SIZE 128

void
basic_test()
{
  printf("--- RUNNING BASIC CORRECTNESS TEST ---\n");
  int fd[2];
  if (pipe(fd) < 0) {
    printf("FAILED: pipe creation failed\n");
    exit(1);
  }

  char *msg = "Hello parallel pipe!";
  int len = strlen(msg);
  if (write(fd[1], msg, len) != len) {
    printf("FAILED: basic write failed\n");
    exit(1);
  }

  char buf[64];
  memset(buf, 0, sizeof(buf));
  if (read(fd[0], buf, len) != len) {
    printf("FAILED: basic read failed\n");
    exit(1);
  }

  if (strcmp(msg, buf) != 0) {
    printf("FAILED: data mismatch. expected '%s', got '%s'\n", msg, buf);
    exit(1);
  }

  close(fd[0]);
  close(fd[1]);
  printf("OK: basic correctness test passed\n");
}

void
parallel_test()
{
  printf("--- RUNNING PARALLEL PRODUCER/CONSUMER TEST ---\n");
  int fd[2];
  if (pipe(fd) < 0) {
    printf("FAILED: pipe creation failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    printf("FAILED: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child: Writer
    close(fd[0]); // Close unused read end

    int total = 0;
    while (total < TEST_SIZE) {
      int to_write = CHUNK_SIZE;
      if (total + to_write > TEST_SIZE) {
        to_write = TEST_SIZE - total;
      }
      char wbuf[CHUNK_SIZE];
      for (int j = 0; j < to_write; j++) {
        wbuf[j] = ((total + j) % 26) + 'A';
      }
      int n = write(fd[1], wbuf, to_write);
      if (n <= 0) {
        printf("FAILED: child write failed at %d bytes\n", total);
        exit(1);
      }
      total += n;
    }
    close(fd[1]);
    exit(0);
  } else {
    // Parent: Reader
    close(fd[1]); // Close unused write end

    char rbuf[CHUNK_SIZE];
    int total = 0;
    while (1) {
      int n = read(fd[0], rbuf, CHUNK_SIZE);
      if (n == 0) {
        // EOF
        break;
      }
      if (n < 0) {
        printf("FAILED: parent read failed at %d bytes\n", total);
        exit(1);
      }
      // Validate contents
      for (int j = 0; j < n; j++) {
        char expected = ((total + j) % 26) + 'A';
        if (rbuf[j] != expected) {
          printf("FAILED: byte mismatch at index %d: expected '%c', got '%c'\n", total + j, expected, rbuf[j]);
          exit(1);
        }
      }
      total += n;
    }

    if (total != TEST_SIZE) {
      printf("FAILED: read length mismatch: expected %d, got %d\n", TEST_SIZE, total);
      exit(1);
    }

    int status;
    wait(&status);
    if (status != 0) {
      printf("FAILED: child writer exited with failure\n");
      exit(1);
    }

    close(fd[0]);
    printf("OK: parallel producer/consumer test passed (%d bytes verified)\n", total);
  }
}

void
multi_writer_test()
{
  printf("--- RUNNING MULTI-WRITER PARALLEL TEST ---\n");
  int fd[2];
  if (pipe(fd) < 0) {
    printf("FAILED: pipe creation failed\n");
    exit(1);
  }

  int pid1 = fork();
  if (pid1 < 0) {
    printf("FAILED: fork 1 failed\n");
    exit(1);
  }

  if (pid1 == 0) {
    // Child 1: Writes 'X's
    close(fd[0]);
    char buf[CHUNK_SIZE];
    memset(buf, 'X', CHUNK_SIZE);
    int total = 0;
    while (total < TEST_SIZE) {
      int to_write = CHUNK_SIZE;
      if (total + to_write > TEST_SIZE) {
        to_write = TEST_SIZE - total;
      }
      int n = write(fd[1], buf, to_write);
      if (n <= 0) exit(1);
      total += n;
    }
    close(fd[1]);
    exit(0);
  }

  int pid2 = fork();
  if (pid2 < 0) {
    printf("FAILED: fork 2 failed\n");
    exit(1);
  }

  if (pid2 == 0) {
    // Child 2: Writes 'Y's
    close(fd[0]);
    char buf[CHUNK_SIZE];
    memset(buf, 'Y', CHUNK_SIZE);
    int total = 0;
    while (total < TEST_SIZE) {
      int to_write = CHUNK_SIZE;
      if (total + to_write > TEST_SIZE) {
        to_write = TEST_SIZE - total;
      }
      int n = write(fd[1], buf, to_write);
      if (n <= 0) exit(1);
      total += n;
    }
    close(fd[1]);
    exit(0);
  }

  // Parent: Reads and counts 'X's and 'Y's
  close(fd[1]);
  char rbuf[CHUNK_SIZE];
  int count_x = 0;
  int count_y = 0;

  while (1) {
    int n = read(fd[0], rbuf, CHUNK_SIZE);
    if (n == 0) break;
    if (n < 0) {
      printf("FAILED: multi-writer read failed\n");
      exit(1);
    }
    for (int j = 0; j < n; j++) {
      if (rbuf[j] == 'X') {
        count_x++;
      } else if (rbuf[j] == 'Y') {
        count_y++;
      } else {
        printf("FAILED: unexpected character '%c' in buffer\n", rbuf[j]);
        exit(1);
      }
    }
  }

  close(fd[0]);

  int s1, s2;
  wait(&s1);
  wait(&s2);

  if (count_x != TEST_SIZE || count_y != TEST_SIZE) {
    printf("FAILED: multi-writer counts mismatch (X: %d, Y: %d)\n", count_x, count_y);
    exit(1);
  }

  printf("OK: multi-writer parallel test passed (%d bytes verified)\n", count_x + count_y);
}

void
closed_pipe_test()
{
  printf("--- RUNNING CLOSED PIPE SENSITIVITY TEST ---\n");
  int fd[2];
  if (pipe(fd) < 0) {
    printf("FAILED: pipe creation failed\n");
    exit(1);
  }

  // Close write end and try reading
  close(fd[1]);
  char c;
  if (read(fd[0], &c, 1) != 0) {
    printf("FAILED: reading from closed write end did not return EOF\n");
    exit(1);
  }
  close(fd[0]);

  // Create pipe, close read end, and try writing
  if (pipe(fd) < 0) {
    printf("FAILED: pipe creation failed\n");
    exit(1);
  }
  close(fd[0]);
  // Writing to a pipe with no readers should return -1 in xv6
  if (write(fd[1], "A", 1) != -1) {
    printf("FAILED: writing to closed read end did not fail\n");
    exit(1);
  }
  close(fd[1]);

  printf("OK: closed pipe sensitivity test passed\n");
}

int
main(int argc, char *argv[])
{
  printf("--- STARTING CONCURRENT PIPE VERIFICATION SUITE ---\n");
  basic_test();
  parallel_test();
  multi_writer_test();
  closed_pipe_test();
  printf("ALL CONCURRENT PIPE TESTS PASSED SUCCESSFULLY!\n");
  exit(0);
}
