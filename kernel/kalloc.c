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
  int count;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; ++i) {
    initlock(&kmem[i].lock, "kmem");
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int id = cpuid();

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  kmem[id].count++;
  release(&kmem[id].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;

  push_off();
  int id = cpuid();

  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r) {
    kmem[id].freelist = r->next;
    kmem[id].count--;
    release(&kmem[id].lock);
    pop_off();
    memset((char*)r, 5, PGSIZE);
    return (void*)r;
  }
  release(&kmem[id].lock);

  int max_id = -1, max_count = 0;
  for(int i = 0; i < NCPU; i++){
    if(i == id) 
      continue;
    acquire(&kmem[i].lock);
    if(kmem[i].count > max_count){
      max_id = i;
      max_count = kmem[i].count;
    }
    release(&kmem[i].lock);
  }

  if(max_id == -1 || max_count < 1){
    pop_off();
    return 0;
  }

  acquire(&kmem[max_id].lock);

  if(kmem[max_id].count < 1){
    release(&kmem[max_id].lock);
    pop_off();
    return 0;
  }

  r = kmem[max_id].freelist;
  kmem[max_id].freelist = r->next;
  kmem[max_id].count--;

  release(&kmem[max_id].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE);
  return (void*)r;
}
