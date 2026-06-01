#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000

struct entry {
  int key;
  int value;
  struct entry *next;
};

struct entry *table[NBUCKET];
int nthread = 1;
int keys[NKEYS];

// Mode of locking
// 0 = No Lock, 1 = Global Lock, 2 = Bucketed Locks (Fine-grained)
int lock_mode = 0; 

pthread_mutex_t global_lock;
pthread_mutex_t bucket_locks[NBUCKET];

double
now()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Thread-safe insert
void
put(int key, int value)
{
  int b = key % NBUCKET;

  if (lock_mode == 1) {
    pthread_mutex_lock(&global_lock);
  } else if (lock_mode == 2) {
    pthread_mutex_lock(&bucket_locks[b]);
  }

  // Insert entry at the head of the bucket chain
  struct entry *new_entry = malloc(sizeof(*new_entry));
  new_entry->key = key;
  new_entry->value = value;
  new_entry->next = table[b];
  table[b] = new_entry;

  if (lock_mode == 1) {
    pthread_mutex_unlock(&global_lock);
  } else if (lock_mode == 2) {
    pthread_mutex_unlock(&bucket_locks[b]);
  }
}

// Thread-safe lookup
struct entry*
get(int key)
{
  int b = key % NBUCKET;

  if (lock_mode == 1) {
    pthread_mutex_lock(&global_lock);
  } else if (lock_mode == 2) {
    pthread_mutex_lock(&bucket_locks[b]);
  }

  struct entry *e = table[b];
  while (e) {
    if (e->key == key) {
      break;
    }
    e = e->next;
  }

  if (lock_mode == 1) {
    pthread_mutex_unlock(&global_lock);
  } else if (lock_mode == 2) {
    pthread_mutex_unlock(&bucket_locks[b]);
  }

  return e;
}

// Worker thread routine for parallel puts
void *
put_worker(void *arg)
{
  long thread_id = (long)arg;
  int chunk = NKEYS / nthread;
  int start = thread_id * chunk;
  int end = start + chunk;
  if (thread_id == nthread - 1) {
    end = NKEYS;
  }

  for (int i = start; i < end; i++) {
    put(keys[i], i);
  }

  return 0;
}

// Worker thread routine for parallel gets
void *
get_worker(void *arg)
{
  long thread_id = (long)arg;
  int chunk = NKEYS / nthread;
  int start = thread_id * chunk;
  int end = start + chunk;
  if (thread_id == nthread - 1) {
    end = NKEYS;
  }

  int missing = 0;
  for (int i = start; i < end; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) {
      missing++;
    }
  }

  return (void *)(long)missing;
}

void
run_benchmark()
{
  pthread_t *threads = malloc(nthread * sizeof(pthread_t));
  double t0, t1;

  // Clear table
  for (int i = 0; i < NBUCKET; i++) {
    struct entry *e = table[i];
    while (e) {
      struct entry *next = e->next;
      free(e);
      e = next;
    }
    table[i] = 0;
  }

  // --- 1. Benchmark Parallel PUTs ---
  t0 = now();
  for (long i = 0; i < nthread; i++) {
    pthread_create(&threads[i], 0, put_worker, (void *)i);
  }
  for (int i = 0; i < nthread; i++) {
    pthread_join(threads[i], 0);
  }
  t1 = now();
  printf("  Put phase completed in %.4f seconds (throughput: %.1f kops/sec)\n", 
         t1 - t0, (NKEYS / 1000.0) / (t1 - t0));

  // --- 2. Benchmark Parallel GETs ---
  int total_missing = 0;
  t0 = now();
  for (long i = 0; i < nthread; i++) {
    pthread_create(&threads[i], 0, get_worker, (void *)i);
  }
  for (int i = 0; i < nthread; i++) {
    void *res;
    pthread_join(threads[i], &res);
    total_missing += (int)(long)res;
  }
  t1 = now();
  printf("  Get phase completed in %.4f seconds (throughput: %.1f kops/sec)\n", 
         t1 - t0, (NKEYS / 1000.0) / (t1 - t0));
  
  if (total_missing > 0) {
    printf("  ---> WARNING: %d keys are missing from the table! (Data Race Occurred)\n", total_missing);
  } else {
    printf("  ---> SUCCESS: 0 keys are missing! (Total Thread-Safety Verified)\n");
  }
  
  free(threads);
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("Usage: %s <number_of_threads>\n", argv[0]);
    exit(1);
  }
  nthread = atoi(argv[1]);
  if (nthread <= 0) {
    printf("Invalid thread count!\n");
    exit(1);
  }

  // Initialize locks
  pthread_mutex_init(&global_lock, 0);
  for (int i = 0; i < NBUCKET; i++) {
    pthread_mutex_init(&bucket_locks[i], 0);
  }

  // Generate unique keys
  srandom(42);
  for (int i = 0; i < NKEYS; i++) {
    keys[i] = random();
  }

  printf("--- PARALLEL HASH TABLE BENCHMARK (Threads: %d, Buckets: %d) ---\n\n", nthread, NBUCKET);

  printf("[Mode 0: No Locking (Unsafe Parallelism)]\n");
  lock_mode = 0;
  run_benchmark();
  printf("\n");

  printf("[Mode 1: Coarse-Grained Locking (Global Lock)]\n");
  lock_mode = 1;
  run_benchmark();
  printf("\n");

  printf("[Mode 2: Fine-Grained Locking (Per-Bucket Locks)]\n");
  lock_mode = 2;
  run_benchmark();
  printf("\n");

  return 0;
}
