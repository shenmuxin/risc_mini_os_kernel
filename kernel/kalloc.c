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
} kmem[NCPU];     // 为每个 CPU 分配独立的 freelist，并用独立的锁保护它。

char *kmem_lock_names[NCPU] = {
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};

void
kinit()
{
  for (int i = 0; i < NCPU; i++){   // 初始化所有锁
    initlock(&kmem[i].lock, kmem_lock_names[i]);
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

  push_off();   // 关闭中断
  int cpu = cpuid();  // 获取当前cpuid
  
  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();    // 开启中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // acquire(&kmem.lock);
  // r = kmem.freelist;  // 取出一个物理页。页表项本身就是物理页。
  // if(r)
  //   kmem.freelist = r->next;
  // release(&kmem.lock);


  // 这种方式可能导致死锁，当cpu_a偷cpu_b,cpu_b同时又偷cpu_a的时候
  push_off();           // 关闭中断
  int cpu = cpuid();    // 获取当前cpu id
  acquire(&kmem[cpu].lock);
  if(!kmem[cpu].freelist) { // no page left for this cpu
    int steal_left = 64; // steal 64 pages from other cpu(s)
    for(int i=0;i<NCPU;i++) {
      if(i == cpu) continue; // no self-robbery
      acquire(&kmem[i].lock);
      struct run *rr = kmem[i].freelist;
      while(rr && steal_left) {
        kmem[i].freelist = rr->next;      // 从第 i 个 CPU 的 freelist 中移除 rr
        rr->next = kmem[cpu].freelist;    // 将 rr 插入到当前 CPU 的 freelist 的开头
        kmem[cpu].freelist = rr;          // 更新当前 CPU 的 freelist 头指针，使其指向 rr
        rr = kmem[i].freelist;            // 更新 rr 指向下一个要移动的内存页
        steal_left--;                     // 减少剩余需要移动的内存页数量
      }
      release(&kmem[i].lock);
      if(steal_left == 0) break; // done stealing
    }
  }
  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);
  pop_off();  // 开启中断


  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;    // 转化为void* 表示该指针可以指向任何类型
}
