#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sysinfo.h"

uint64
sys_sysinfo(void)
{
  uint64 addr;
  argaddr(0, &addr);

  struct sysinfo si;
  si.freemem = free_kmem_size();
  si.nproc = process_numbers();

  return copyout(myproc()->pagetable, addr, (char *) &si, sizeof(si));
}