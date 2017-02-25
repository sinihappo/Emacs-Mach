#include <stdio.h>
#include <mach.h>
#include "lisp.h"

vm_address_t	mach_first_address = 16*1024*1024;

vm_address_t	mach_find_hole();

Lisp_Object Vmach_alloc_dir;

int	use_mach_alloc;

typedef struct mach_alloc	mach_alloc, *mach_alloc_ptr;

struct mach_alloc {
  mach_alloc_ptr	link;
  int			vm_allocated;
  char			*p;
  vm_address_t		addr;
  vm_size_t		size;
  int			rsize;
  char			*filename;
};

mach_alloc_ptr	mach_alloc_root;

static int
mach_roundup(n)

unsigned int	n;

{
  int	i;
  unsigned int	x;

  for(i = 0, x = 8192; i < 32 && x; x += x, i++) {
    if (n < x)
      return x;
  }
  fatal("Cannot mach_roundup n=0x%08x\n",n);
  exit(1);
  return 0;
}

mach_alloc_ptr
find_mach_alloc(p0,rootptr)

char	*p0;
mach_alloc_ptr	**rootptr;

{
  mach_alloc_ptr	p;
  mach_alloc_ptr	*pp;

  for(pp = &mach_alloc_root; p = *pp; pp = &p->link) {
    if (p->p == p0) {
      if (rootptr) *rootptr = pp;
      return p;
    }
  }
  fatal("Cannot mach_alloc()\n");
  exit(1);
}

char *
buf_malloc(n)

int	n;

{
  mach_alloc_ptr	p;
  vm_address_t		a;
  vm_size_t		n2;

  p = (mach_alloc_ptr)xmalloc(sizeof(*p));
  if (p->vm_allocated = use_mach_alloc) {
    n2 = mach_roundup(n);
    a = mach_find_hole(n2);
    if (vm_allocate(mach_task_self(),&a,n2,FALSE) != KERN_SUCCESS) {
      fatal("Cannot vm_allocate %d bytes\n",n2);
      exit(1);
    }
  } else {
    n2 = n;
    a = (vm_address_t)xmalloc(n);
  }
  p->addr = a;
  p->p = (char*)a;
  p->size = n2;
  p->rsize = n;
  p->link = mach_alloc_root;
  mach_alloc_root = p;
  return p->p;
}

char *
buf_realloc(p0,n)

char	*p0;
int	n;

{
  mach_alloc_ptr	p;
  char			*p2;
  int			size;

  if (p0 == 0)
    return buf_malloc(n);
  p = find_mach_alloc(p0,(mach_alloc_ptr**)0);
  if (n < p->size) {
    p->rsize = n;
    return p0;
  }
  p2 = buf_malloc(n);
  if (p->vm_allocated && use_mach_alloc) {
    if (vm_copy(mach_task_self(),p->addr,p->size,p2) != KERN_SUCCESS) {
      fatal("Cannot vm_copy from 0x%08x to 0x%08x\n",p->addr,p2);
      exit(1);
    }
  } else {
    bcopy(p0,p2,p->rsize);
  }
  buf_free(p0);
  return p2;
}

int
buf_free(p0)

char	*p0;

{
  mach_alloc_ptr	p;
  mach_alloc_ptr	*pp;
  char			*p2;

  if (p0 == 0)
    return 0;
  p = find_mach_alloc(p0,&pp);

  if (p->vm_allocated) {
    if (vm_deallocate(mach_task_self(),p->addr,p->size) != KERN_SUCCESS) {
      fatal("Cannot vm_deallocate\n");
      exit(1);
    }
  } else {
    free(p0);
  }
  *pp = p->link;
  free(p);
  return 0;
}

int
buf_malloc_init(start,warnfun)

{
  return 0;
}

vm_address_t
mach_find_hole(n)

{
  task_t	task;
  kern_return_t	e;
  int		i;
  vm_address_t	a;
  vm_address_t	pa;
  vm_address_t	max_a;
  vm_size_t	len;
  vm_prot_t	prot;
  vm_prot_t	max_prot;
  vm_inherit_t	inher;
  boolean_t	shared;
  mach_port_t	name;
  vm_offset_t	off;
  int		sum;

  max_a = 0x80000000;


  task = mach_task_self();
  for(a = mach_first_address, pa = a; a < max_a; pa = (a += len)) {
    e = vm_region(task,
		  &a,&len,&prot,&max_prot,&inher,&shared,&name,&off);
    if (e != KERN_SUCCESS) {
      fatal("Error %s\n",mach_error_string(e));
      break;
    }
    if (name != MACH_PORT_NULL)
      mach_port_deallocate(task,name);
    if (a-pa >= n)
      return pa;
  }
  return pa;
}

syms_of_mach_alloc ()
{
  DEFVAR_BOOL ("mach-alloc", &use_mach_alloc,
    "*Non-nil means buffer memory is allocated using vm_allocate.");
  use_mach_alloc = 0;
  DEFVAR_LISP ("mach-alloc-dir", &Vmach_alloc_dir,
	       "*This string name the directory, where buffer paging files are created.");
}
