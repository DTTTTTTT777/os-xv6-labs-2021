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

/*struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;*/

//Added----------------------------------beg
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; // 为每个 CPU 分配独立的kmem 
//Added----------------------------------end

void
kinit()
{
  //initlock(&kmem.lock, "kmem");
  
  //Added----------------------------------beg
  char kmem_name[32];
  for (int i = 0; i < NCPU; i++) {
      snprintf(kmem_name, 32, "kmem_%d", i);
      initlock(&kmem[i].lock, kmem_name); //init all locks
  }
  //Added----------------------------------end
  
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

// Free the page of physical memory pointed at by v,
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

  /*acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);*/
  
  
  //Added------------------------------beg
  push_off();

  int cpu = cpuid();

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();
  //Added------------------------------end
  
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  /*acquire(&kmem.lock);
  r = kmem.freelist;*/
  
  //Added
  push_off();
  int CPUID = cpuid();
  acquire(&kmem[CPUID].lock);
  r = kmem[CPUID].freelist;
  
  if(r)
    //kmem.freelist = r->next;
    kmem[CPUID].freelist = r->next; //Added
    
  if (r == 0) { // 若当前CPU上没有空闲页
        // 在其他CPU上查找空闲页
        for (int i = 0; i < NCPU; i++) {
            if (i == CPUID)
                continue;
            // 获取其他CPU的锁
            acquire(&kmem[i].lock);
            r = kmem[i].freelist;
            if (r)
                kmem[i].freelist = r->next;
            release(&kmem[i].lock);
            if (r) // 已找到
                break;
        }
    }

    //release(&kmem.lock); 
    
    release(&kmem[CPUID].lock);
    pop_off();
  

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
