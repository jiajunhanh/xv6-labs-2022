// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "reference_count.h"

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

struct reference_count kmem_reference_counts[(PHYSTOP + PGSIZE) / PGSIZE];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  for (int i = 0; i < (PHYSTOP + PGSIZE) / PGSIZE; ++i) {
    struct reference_count *refctn = &kmem_reference_counts[i];
    initlock(&refctn->lock, "reference_count");
    refctn->cnt = 1;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  struct reference_count *refcnt = &kmem_reference_counts[(uint64) pa / PGSIZE];

  acquire(&refcnt->lock);

  if (refcnt->cnt == 0) {
    panic("kfree: cnt == 0");
  }
  refcnt->cnt -= 1;

  if (refcnt->cnt) {
    release(&refcnt->lock);
    return;
  }

  release(&refcnt->lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    memset((char *) r, 5, PGSIZE); // fill with junk
    kmem_reference_counts[(uint64) r / PGSIZE].cnt = 1;
  }

  return (void*)r;
}

uint64 free_kmem_size(void) {
  uint64 freemem = 0;

  acquire(&kmem.lock);

  struct run *r = kmem.freelist;
  while (r) {
    freemem += PGSIZE;
    r = r->next;
  }

  release(&kmem.lock);

  return freemem;
}
