#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
run_basic_pingpong()
{
  int p_parent_to_child[2];
  int p_child_to_parent[2];
  char buf[1] = {'x'};

  // Create both pipes
  if (pipe(p_parent_to_child) < 0 || pipe(p_child_to_parent) < 0) {
    printf("pingpong: pipe creation failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    printf("pingpong: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // === CHILD PROCESS ===
    // Close unused ends:
    // Read from p_parent_to_child[0]
    // Write to p_child_to_parent[1]
    close(p_parent_to_child[1]);
    close(p_child_to_parent[0]);

    // Read the byte sent by the parent
    if (read(p_parent_to_child[0], buf, 1) != 1) {
      printf("pingpong: child read failed\n");
      exit(1);
    }
    printf("%d: received ping\n", getpid());

    // Write the byte back to the parent
    if (write(p_child_to_parent[1], buf, 1) != 1) {
      printf("pingpong: child write failed\n");
      exit(1);
    }

    close(p_parent_to_child[0]);
    close(p_child_to_parent[1]);
    exit(0);
  } else {
    // === PARENT PROCESS ===
    // Close unused ends:
    // Write to p_parent_to_child[1]
    // Read from p_child_to_parent[0]
    close(p_parent_to_child[0]);
    close(p_child_to_parent[1]);

    // Write a byte to the child
    if (write(p_parent_to_child[1], buf, 1) != 1) {
      printf("pingpong: parent write failed\n");
      exit(1);
    }

    // Wait and read the byte back from the child
    if (read(p_child_to_parent[0], buf, 1) != 1) {
      printf("pingpong: parent read failed\n");
      exit(1);
    }
    printf("%d: received pong\n", getpid());

    close(p_parent_to_child[1]);
    close(p_child_to_parent[0]);
    
    // Reap the child process
    wait(0);
    exit(0);
  }
}

void
run_performance_test(int rounds)
{
  int p_parent_to_child[2];
  int p_child_to_parent[2];
  char buf[1] = {'x'};

  if (pipe(p_parent_to_child) < 0 || pipe(p_child_to_parent) < 0) {
    printf("pingpong: pipe creation failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    printf("pingpong: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // === CHILD PROCESS ===
    close(p_parent_to_child[1]);
    close(p_child_to_parent[0]);

    for (int i = 0; i < rounds; i++) {
      if (read(p_parent_to_child[0], buf, 1) != 1) {
        printf("pingpong performance: child read failed at round %d\n", i);
        exit(1);
      }
      if (write(p_child_to_parent[1], buf, 1) != 1) {
        printf("pingpong performance: child write failed at round %d\n", i);
        exit(1);
      }
    }

    close(p_parent_to_child[0]);
    close(p_child_to_parent[1]);
    exit(0);
  } else {
    // === PARENT PROCESS ===
    close(p_parent_to_child[0]);
    close(p_child_to_parent[1]);

    printf("Starting performance test: %d exchanges...\n", rounds);
    uint64 start_ticks = uptime();

    for (int i = 0; i < rounds; i++) {
      if (write(p_parent_to_child[1], buf, 1) != 1) {
        printf("pingpong performance: parent write failed at round %d\n", i);
        exit(1);
      }
      if (read(p_child_to_parent[0], buf, 1) != 1) {
        printf("pingpong performance: parent read failed at round %d\n", i);
        exit(1);
      }
    }

    uint64 end_ticks = uptime();
    uint64 total_ticks = end_ticks - start_ticks;

    close(p_parent_to_child[1]);
    close(p_child_to_parent[0]);
    wait(0);

    printf("Test finished.\n");
    printf("Total ticks elapsed: %ld\n", total_ticks);
    
    // In xv6, timer interrupts trigger 10 times per second (10 ticks = 1 second)
    if (total_ticks == 0) {
      printf("Error: Elapsed time was less than 1 tick (100ms). Try increasing the number of rounds.\n");
    } else {
      // 1 tick = 0.1 seconds.
      // Total seconds = total_ticks / 10.0
      // Exchanges per second = rounds / (total_ticks / 10.0) = (rounds * 10) / total_ticks
      uint64 rate = (rounds * 10) / total_ticks;
      printf("Performance: %ld exchanges/second\n", rate);
    }
    
    exit(0);
  }
}

int
main(int argc, char *argv[])
{
  if (argc > 1) {
    int rounds = atoi(argv[1]);
    if (rounds <= 0) {
      printf("Usage: pingpong [number_of_performance_rounds]\n");
      exit(1);
    }
    run_performance_test(rounds);
  } else {
    run_basic_pingpong();
  }
  exit(0);
}
