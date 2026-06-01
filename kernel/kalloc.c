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

// Statically allocate kmem buckets and spinlocks for all NCPU cores
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

// Shared reference counting structure for Copy-on-Write (COW) fork
struct {
  struct spinlock lock;
  int count[(PHYSTOP - KERNBASE) / PGSIZE];
} kref;

void
kinit()
{
  // Initialize spinlocks for each core freelist
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
  }
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
// call to kalloc(). (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Handle COW reference counting: only free page if count drops to 0
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

  // Disable hardware interrupts to get a stable CPU ID and prevent scheduler preemption
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off(); // Restore interrupts
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// Implements Work Stealing to reclaim memory from other CPU free lists if the local list is empty.
void *
kalloc(void)
{
  struct run *r = 0;

  // Disable hardware interrupts to obtain stable cpuid
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  
  r = kmem[id].freelist;
  if (r) {
    // Found page locally
    kmem[id].freelist = r->next;
  } else {
    // Work Stealing: Loop through other cores and steal a page if found
    for (int i = 0; i < NCPU; i++) {
      if (i == id)
        continue;
      
      acquire(&kmem[i].lock);
      struct run *stolen = kmem[i].freelist;
      if (stolen) {
        kmem[i].freelist = stolen->next;
        release(&kmem[i].lock);
        r = stolen;
        break;
      }
      release(&kmem[i].lock);
    }
  }
  
  release(&kmem[id].lock);
  pop_off(); // Restore interrupts

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
// Iterates and sums page sizes across all kmem buckets under sequential spinlocks
uint64
count_free_bytes(void)
{
  uint64 free_pages = 0;

  for (int i = 0; i < NCPU; i++) {
    acquire(&kmem[i].lock);
    struct run *r = kmem[i].freelist;
    while (r) {
      free_pages++;
      r = r->next;
    }
    release(&kmem[i].lock);
  }

  return free_pages * PGSIZE; // Map page count to bytes (4096 bytes per page)
}
