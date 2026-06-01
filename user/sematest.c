#include "kernel/types.h"
#include "user/user.h"

void
test_basic(void)
{
  printf("--- SEMAPHORE BASIC TEST START ---\n");
  
  // Semaphore 0: initial value 1 (mutex)
  if (seminit(0, 1) < 0) {
    printf("sem_init failed\n");
    exit(1);
  }

  printf("Acquiring semaphore 0...\n");
  semdown(0);
  printf("Semaphore 0 acquired!\n");

  printf("Releasing semaphore 0...\n");
  semup(0);
  printf("Semaphore 0 released!\n");
  
  printf("--- SEMAPHORE BASIC TEST PASSED ---\n\n");
}

void
test_producer_consumer(void)
{
  printf("--- SEMAPHORE PRODUCER-CONSUMER TEST START ---\n");
  
  // Semaphore 1: empty slots (initially 3)
  // Semaphore 2: full slots (initially 0)
  // Semaphore 3: mutex (initially 1)
  if (seminit(1, 3) < 0 || seminit(2, 0) < 0 || seminit(3, 1) < 0) {
    printf("sem_init failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Producer process
    for (int i = 0; i < 5; i++) {
      semdown(1); // Wait for empty slot
      semdown(3); // Enter critical section
      printf("[Producer] Producing item %d...\n", i);
      semup(3);   // Exit critical section
      semup(2);   // Signal full slot
    }
    exit(0);
  } else {
    // Consumer process
    for (int i = 0; i < 5; i++) {
      semdown(2); // Wait for full slot
      semdown(3); // Enter critical section
      printf("[Consumer] Consuming item %d...\n", i);
      semup(3);   // Exit critical section
      semup(1);   // Signal empty slot
    }
    wait(0);
  }

  printf("--- SEMAPHORE PRODUCER-CONSUMER TEST PASSED ---\n\n");
}

void
test_barrier(void)
{
  printf("--- SEMAPHORE BARRIER TEST START ---\n");

  // Semaphore 4: barrier arrival (initially 0)
  // Semaphore 5: mutex for counter (initially 1)
  if (seminit(4, 0) < 0 || seminit(5, 1) < 0) {
    printf("sem_init failed\n");
    exit(1);
  }

  // We will fork 3 child processes that will synchronize at a barrier
  int num_workers = 3;
  
  // A shared counter is not directly possible in separate process spaces in xv6 without shared memory,
  // but we can use semaphores to coordinate process sequencing:
  // worker 1 done -> signals sem 4
  // worker 2 done -> signals sem 4
  // parent waits for both signals
  for (int i = 0; i < num_workers; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      printf("[Worker %d] Starting work...\n", i);
      pause(10); // Simulate some work
      printf("[Worker %d] Reached barrier, signaling parent...\n", i);
      semup(4); // Signal arrival
      exit(0);
    }
  }

  // Parent blocks until all workers reach the barrier
  printf("[Parent] Waiting for %d workers at the barrier...\n", num_workers);
  for (int i = 0; i < num_workers; i++) {
    semdown(4);
  }
  printf("[Parent] All workers arrived! Releasing barrier...\n");

  // Reclaim all child zombies
  for (int i = 0; i < num_workers; i++) {
    wait(0);
  }

  printf("--- SEMAPHORE BARRIER TEST PASSED ---\n\n");
}

int
main(void)
{
  printf("--- RUNNING SEMAPHORE DIAGNOSTIC SUITE ---\n");
  test_basic();
  test_producer_consumer();
  test_barrier();
  printf("ALL SEMAPHORE TESTS PASSED SUCCESSFULLY!\n");
  exit(0);
}
