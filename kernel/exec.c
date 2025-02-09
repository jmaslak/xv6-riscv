#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

static int loadseg(pde_t *, uint64, struct inode *, uint, uint);

int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();
  char potential[MAXPATH+3];
  int iteration_count = 0;
  int len;

  // We iterate rather than recursively call exec, because we're
  // allocating a relatively big local variable (potential) and we'd be
  // limited on stack depth if we recursively called exec too many
  // times. We use this label as the start point of our iteration.
start_exec:
  // If we're > 4 deep in recursion, we'll bail out, just like Linux.
  if (iteration_count > 4) {
      kfree(argv);
      return -1;
  }

  begin_op();
  if((ip = namei(path)) == 0){
    end_op();
    goto bad;
  }
  ilock(ip);

  // Check for shebang. To save on reads (since we're reading every
  // non-shebang file twice already), we grab enough of the file to get
  // any potential filename.
  if((len = readi(ip, 0, (uint64)&potential, 0, sizeof(potential))) >= 3) {
    if ((potential[0] == '#') && (potential[1] == '!')) {
      iunlockput(ip);  // We are done with the old inode at this point.
                       // This does create a potential race/security condition,
                       // but this race condition exists in Linux too!
                       // I.E. if someone replaces the shell script
                       // in-between the exec() call start and the
                       // interpreter opening argv[1], the interpreter
                       // may run something other than would normally be
                       // expected.
      end_op();
      ip = 0;
      potential[MAXPATH+2] = '\0';

      for (i=2; i<=len; i++) {
        if ((potential[i] == '\n') || potential[i] == '\0') {
          potential[i] = '\0';
          char * arg = kalloc();
          if (arg == 0)
            goto bad;

          safestrcpy(arg, potential+2, MAXPATH+1);

          char ** newargs = kalloc();
          if (newargs == 0) {
            kfree(arg);
            goto bad;
          }

          newargs[0] = arg;
          for(argc = 0; argv[argc]; argc++) {
            // number of arguments is too large to fit on a page, along
            // with zero arg.
            if ((uint64)(newargs + PGSIZE) <= sizeof(char **) + (uint64)(newargs+argc+2)) {
              kfree(arg);
              kfree(newargs);
              goto bad;
            }
            newargs[argc+1] = argv[argc];  // We create a new argv with
                                           // interpreter as the first
                                           // arg
          }
          newargs[argc+1] = 0;
          if (iteration_count)
            kfree(argv);
          argv = newargs;
          argv[0] = arg;
          path = arg;
          iteration_count++;
          goto start_exec;
        }
      }
      // The filename was too long, so error out.
      goto bad;
    }
  }

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate some pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the rest as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-(USERSTACK+1)*PGSIZE);
  sp = sz;
  stackbase = sp - USERSTACK*PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  // Clean up the pages we allocated.
  if (iteration_count) {
    for (i=0; i<iteration_count; i++)
      kfree(argv[i]);
    kfree(argv);
  }

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  // Clean up pages we allocated
  if (iteration_count) {
    for (i=0; i<iteration_count; i++)
      kfree(argv[i]);
    kfree(argv);
  }

  // Other cleanup
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}
