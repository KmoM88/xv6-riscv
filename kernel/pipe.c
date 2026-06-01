#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock read_lock;
  struct spinlock write_lock;
  char data[PIPESIZE];
  uint nread;    // number of bytes read
  uint nwrite;   // number of bytes written
  int readopen;  // read fd is still open
  int writeopen; // write fd is still open
};

int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *pi;

  pi = 0;
  *f0 = *f1 = 0;
  if ((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  if ((pi = (struct pipe *)kalloc()) == 0)
    goto bad;
  pi->readopen = 1;
  pi->writeopen = 1;
  pi->nwrite = 0;
  pi->nread = 0;
  initlock(&pi->read_lock, "piperead");
  initlock(&pi->write_lock, "pipewrite");
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = pi;
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = pi;
  return 0;

bad:
  if (pi)
    kfree((char *)pi);
  if (*f0)
    fileclose(*f0);
  if (*f1)
    fileclose(*f1);
  return -1;
}

void
pipeclose(struct pipe *pi, int writable)
{
  acquire(&pi->read_lock);
  acquire(&pi->write_lock);
  if (writable) {
    pi->writeopen = 0;
    wakeup(&pi->nread);
  } else {
    pi->readopen = 0;
    wakeup(&pi->nwrite);
  }
  if (pi->readopen == 0 && pi->writeopen == 0) {
    release(&pi->write_lock);
    release(&pi->read_lock);
    kfree((char *)pi);
  } else {
    release(&pi->write_lock);
    release(&pi->read_lock);
  }
}

int
pipewrite(struct pipe *pi, uint64 addr, int n)
{
  int i = 0;
  struct proc *pr = myproc();

  acquire(&pi->write_lock);
  while (i < n) {
    if (pi->readopen == 0 || killed(pr)) {
      release(&pi->write_lock);
      return -1;
    }
    if (pi->nwrite == pi->nread + PIPESIZE) { //DOC: pipewrite-full
      // Wake up the reader since the pipe is full and we need them to read.
      // Release write_lock first to avoid deadlock.
      release(&pi->write_lock);
      acquire(&pi->read_lock);
      wakeup(&pi->nread);
      release(&pi->read_lock);

      acquire(&pi->write_lock);
      // Re-check conditions after re-acquiring the lock
      if (pi->readopen == 0 || killed(pr)) {
        release(&pi->write_lock);
        return i > 0 ? i : -1;
      }
      if (pi->nwrite == pi->nread + PIPESIZE) {
        sleep(&pi->nwrite, &pi->write_lock);
      }
    } else {
      char ch;
      if (copyin(pr->pagetable, &ch, addr + i, 1) == -1)
        break;
      pi->data[pi->nwrite % PIPESIZE] = ch;
      pi->nwrite++;
      i++;
    }
  }
  release(&pi->write_lock);

  // Wake up reader since we wrote some bytes.
  if (i > 0) {
    acquire(&pi->read_lock);
    wakeup(&pi->nread);
    release(&pi->read_lock);
  }

  return i;
}

int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->read_lock);
  while (pi->nread == pi->nwrite && pi->writeopen) { //DOC: pipe-empty
    if (killed(pr)) {
      release(&pi->read_lock);
      return -1;
    }
    sleep(&pi->nread, &pi->read_lock); //DOC: piperead-sleep
  }
  for (i = 0; i < n; i++) { //DOC: piperead-copy
    if (pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread % PIPESIZE];
    if (copyout(pr->pagetable, addr + i, &ch, 1) == -1) {
      if (i == 0)
        i = -1;
      break;
    }
    pi->nread++;
  }
  release(&pi->read_lock);

  // Wake up writer since we read some bytes and freed up space.
  if (i > 0) {
    acquire(&pi->write_lock);
    wakeup(&pi->nwrite);
    release(&pi->write_lock);
  }

  return i;
}

