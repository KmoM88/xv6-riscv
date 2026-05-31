// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int count[(PHYSTOP - KERNBASE) / PGSIZE];
} kref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kref.lock, "kref");
  memset(kref.count, 0, sizeof(kref.count));
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 idx = ((uint64)pa - KERNBASE) / PGSIZE;
  acquire(&kref.lock);
  if (kref.count[idx] > 1) {
    kref.count[idx]--;
    release(&kref.lock);
    return;
  }
  kref.count[idx] = 0;
  release(&kref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    memset((char *)r, 5, PGSIZE); // fill with junk
    uint64 idx = ((uint64)r - KERNBASE) / PGSIZE;
    acquire(&kref.lock);
    kref.count[idx] = 1;
    release(&kref.lock);
  }
  return (void *)r;
}

void
incref(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    return;
  uint64 idx = ((uint64)pa - KERNBASE) / PGSIZE;
  acquire(&kref.lock);
  kref.count[idx]++;
  release(&kref.lock);
}


// Return the total number of free memory bytes
uint64
count_free_bytes(void)
{
  struct run *r;
  uint64 free_pages = 0;

  acquire(&kmem.lock);
  r = kmem.freelist;
  while (r) {
    free_pages++;
    r = r->next;
  }
  release(&kmem.lock);

  return free_pages * PGSIZE; // Map page count to bytes (4096 bytes per page)
}

