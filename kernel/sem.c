#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"

#define MAX_SEM 16

struct sem {
  struct spinlock lock;
  int count;
  int active;
};

struct sem semaphores[MAX_SEM];

void
semainit(void)
{
  for (int i = 0; i < MAX_SEM; i++) {
    initlock(&semaphores[i].lock, "semaphore");
    semaphores[i].active = 0;
  }
}

int
sema_init(int id, int value)
{
  if (id < 0 || id >= MAX_SEM || value < 0)
    return -1;

  acquire(&semaphores[id].lock);
  semaphores[id].count = value;
  semaphores[id].active = 1;
  release(&semaphores[id].lock);
  return 0;
}

int
sema_down(int id)
{
  if (id < 0 || id >= MAX_SEM)
    return -1;

  struct sem *s = &semaphores[id];
  acquire(&s->lock);
  if (!s->active) {
    release(&s->lock);
    return -1;
  }

  struct proc *p = myproc();
  while (s->count <= 0) {
    if (killed(p)) {
      release(&s->lock);
      return -1;
    }
    sleep(s, &s->lock);
  }
  s->count--;
  release(&s->lock);
  return 0;
}

int
sema_up(int id)
{
  if (id < 0 || id >= MAX_SEM)
    return -1;

  struct sem *s = &semaphores[id];
  acquire(&s->lock);
  if (!s->active) {
    release(&s->lock);
    return -1;
  }

  s->count++;
  wakeup(s);
  release(&s->lock);
  return 0;
}
