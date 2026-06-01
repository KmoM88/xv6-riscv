#include "kernel/types.h"
#include "user/user.h"

/* Max number of threads */
#define MAX_THREADS 4
#define STACK_SIZE 8192

struct thread_context {
  uint64 ra;
  uint64 sp;
  /* Callee-saved registers */
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

enum thread_state { FREE, RUNNABLE, RUNNING };

struct thread {
  char stack[STACK_SIZE];          /* Thread stack */
  enum thread_state state;         /* FREE, RUNNABLE, RUNNING */
  struct thread_context context;   /* Saved context registers */
};

struct thread all_threads[MAX_THREADS];
struct thread *current_thread;
extern void thread_switch(struct thread_context *old, struct thread_context *next);

void 
thread_init(void)
{
  // The main user program execution itself becomes thread 0
  all_threads[0].state = RUNNING;
  current_thread = &all_threads[0];
}

void 
thread_schedule(void)
{
  struct thread *t, *next = 0;
  
  // Look for a RUNNABLE thread starting from the next slot to be fair
  int start_idx = (current_thread - all_threads + 1) % MAX_THREADS;
  for (int i = 0; i < MAX_THREADS; i++) {
    int idx = (start_idx + i) % MAX_THREADS;
    if (all_threads[idx].state == RUNNABLE) {
      next = &all_threads[idx];
      break;
    }
  }

  if (next == 0) {
    // If we are currently in a user thread and the main thread is still active, return to the main thread
    if (current_thread != &all_threads[0] && all_threads[0].state == RUNNING) {
      struct thread *old = current_thread;
      next = &all_threads[0];
      current_thread = next;
      thread_switch(&old->context, &next->context);
      return;
    }
    
    // Otherwise, we have no threads to schedule!
    printf("uthread: no runnable threads left!\n");
    exit(0);
  }

  if (next) {
    struct thread *old = current_thread;
    next->state = RUNNING;
    current_thread = next;
    thread_switch(&old->context, &next->context);
  }
}

void 
thread_create(void (*func)())
{
  struct thread *t = 0;

  for (int i = 0; i < MAX_THREADS; i++) {
    if (all_threads[i].state == FREE) {
      t = &all_threads[i];
      break;
    }
  }

  if (t == 0) {
    printf("uthread: thread pool full!\n");
    return;
  }

  memset(t->stack, 0, sizeof(t->stack));
  t->state = RUNNABLE;

  // Set up context registers
  // Stack grows downwards, so sp starts at the end of the stack array
  uint64 sp = (uint64)&t->stack[STACK_SIZE];
  sp = sp - (sp % 16); // 16-byte stack alignment required by RISC-V calling convention
  
  t->context.ra = (uint64)func;
  t->context.sp = sp;
}

void 
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}

// Exit current thread
void 
thread_exit(void)
{
  current_thread->state = FREE;
  thread_schedule();
}

// --- Diagnostic Test Threads ---
volatile int count_a = 0;
volatile int count_b = 0;

void 
thread_a(void)
{
  for (int i = 0; i < 5; i++) {
    count_a++;
    printf("Thread A: active (iteration %d, count_a=%d)\n", i + 1, count_a);
    thread_yield();
  }
  printf("Thread A: completing and exiting\n");
  thread_exit();
}

void 
thread_b(void)
{
  for (int i = 0; i < 5; i++) {
    count_b++;
    printf("Thread B: active (iteration %d, count_b=%d)\n", i + 1, count_b);
    thread_yield();
  }
  printf("Thread B: completing and exiting\n");
  thread_exit();
}

int 
main(int argc, char *argv[])
{
  printf("--- USER-LEVEL THREADS (uthread) START ---\n");
  thread_init();
  
  printf("[uthread: creating Thread A and Thread B...]\n");
  thread_create(thread_a);
  thread_create(thread_b);
  
  printf("[uthread: starting scheduler loop...]\n");
  thread_schedule();
  
  printf("--- USER-LEVEL THREADS (uthread) END ---\n");
  exit(0);
}
