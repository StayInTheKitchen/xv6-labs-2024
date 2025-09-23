#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_mmap(void)
{
  struct proc *p = myproc();

  uint64 addr;
  int len;
  int prot;
  int flags;
  int fd;
  int offset;

  argaddr(0, &addr);
  argint(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, &offset);

  struct file *fp;
  if(fd < 0 || fd >= NOFILE || (fp=p->ofile[fd]) == 0)
    return -1;
  

  if (is_readonly(fp) && (prot & PROT_WRITE) && (flags & MAP_SHARED)) {
    return -1;
  }
  if (is_writeonly(fp) && (prot & PROT_READ) && (flags & MAP_SHARED)) {
    return -1;
  }

  uint64 file_size = get_filesize(fp);

  int i;
  for (i = 0; i < 16; ++i) {
    if (p->vma[i].used != 0)
      continue;

    p->vma[i].used = 1;

    p->vma[i].start = p->vma_start;
    p->vma[i].len = len;

    p->vma[i].file_start = p->vma_start;
    p->vma[i].file_end = p->vma_start + file_size;

    filedup(fp);
    p->vma[i].fp = fp;
    p->vma[i].offset = offset;

    p->vma[i].prot = prot;
    p->vma[i].flags = flags;
    break;
  }

  if (i == 16)
    return -1; 

  uint64 ret = p->vma_start;
  p->vma_start += len;

  return ret;
}

uint64
sys_munmap(void)
{
  struct proc *p = myproc();
  uint64 addr;
  int len;

  argaddr(0, &addr);
  argint(1, &len);

  uint64 mmap_start;
  uint64 mmap_end;

  int i;
  for (i = 0; i < 16; ++i) {
    if (p->vma[i].used == 0)
      continue;
    
    mmap_start = p->vma[i].start;
    mmap_end = mmap_start + p->vma[i].len;

    if (mmap_start <= addr && addr < mmap_end)
      break;
  }

  if (i == 16)
    panic("munmap: VMA not found");

  if (p->vma[i].flags & MAP_SHARED) {
    uint64 va;

    int left = 0;
    for (va = addr; va < PGROUNDDOWN(addr + len); va += PGSIZE) {
      if (va == PGROUNDDOWN(p->vma[i].file_end)) {
        left = 1;
        break;
      }

      struct inode *ip = getifromvma(p->vma[i]);
      begin_op();
      ilock(ip);
      writei(ip, 1, va, va - p->vma[i].file_start, PGSIZE);
      iunlock(ip);
      end_op();
    }

    if (left == 1) {
      struct inode *ip = getifromvma(p->vma[i]);
      begin_op();
      ilock(ip);
      writei(ip, 1, va, va - p->vma[i].file_start, p->vma[i].file_end - va);
      iunlock(ip);
      end_op();
    }
  }

  int npages = len / PGSIZE;

  uint64 va = addr;
  for (int i = 0; i < npages; ++i) {
    pte_t *pte = walk(p->pagetable, va, 0);

    if (pte == 0 || *pte == 0)
      continue;

    uvmunmap(p->pagetable, va, 1, 1);
    va += PGSIZE;
  }

  if (addr == p->vma[i].start) {
    p->vma[i].start += len;
  }
  p->vma[i].len -= len;

  if (p->vma[i].len == 0) {
    fileclose(p->vma[i].fp);
    p->vma[i].used = 0;
  }

  return 0;
}