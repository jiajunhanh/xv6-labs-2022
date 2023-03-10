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
  char lock_name[6];
} kmem[NCPU];

uint kmem_reference_counts[(PHYSTOP + PGSIZE) / PGSIZE];

void
kinit()
{
  for (int i = 0; i < NCPU; ++i) {
    snprintf(kmem[i].lock_name, 5, "kmem%d", i);
    initlock(&kmem[i].lock, kmem[i].lock_name);
  }

  for (int i = 0; i < (PHYSTOP + PGSIZE) / PGSIZE; ++i) {
    kmem_reference_counts[i] = 1;
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

  uint *refcnt = &kmem_reference_counts[(uint64) pa / PGSIZE];

  if (__sync_sub_and_fetch(refcnt, 1)) {
    if (*refcnt + 1 == 0) {
      panic("kfree: cnt == 0");
    }
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  int i = cpuid();
  acquire(&kmem[i].lock);
  r->next = kmem[i].freelist;
  kmem[i].freelist = r;
  release(&kmem[i].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();

  for (int j = 0; j < NCPU; ++j) {
    int i = (cpuid() + j) % NCPU;
    acquire(&kmem[i].lock);
    r = kmem[i].freelist;

    if (r) {
      kmem[i].freelist = r->next;
      release(&kmem[i].lock);
      break;
    }

    release(&kmem[i].lock);
  }

  pop_off();

  if (r) {
    memset((char *) r, 5, PGSIZE); // fill with junk
    uint *refcnt = &kmem_reference_counts[(uint64) r / PGSIZE];
    if (__sync_fetch_and_add(refcnt, 1)) {
      panic("kalloc: cnt != 0");
    }
  }

  return (void*)r;
}

uint64 free_kmem_size(void) {
  uint64 freemem = 0;

  for (int i = 0; i < NCPU; ++i) {
    acquire(&kmem[i].lock);
    struct run *r = kmem[i].freelist;

    while (r) {
      freemem += PGSIZE;
      r = r->next;
    }

    release(&kmem[i].lock);
  }

  return freemem;
}
