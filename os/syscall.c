#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	struct proc *p = curr_proc();

	TimeVal *kern_val = (TimeVal *)useraddr(p->pagetable, (uint64)val);

	uint64 cycle = get_cycle();

	kern_val->sec = cycle / CPU_FREQ;
	kern_val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;

	// uint64 cycle = get_cycle();
	// val->sec = cycle / CPU_FREQ;
	// val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	 
	return 0;
}

// TODO: add support for mmap and munmap syscall.
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd) {
	if (len == 0 || len > (1024 * 1024 * 1024)) {
		return -1;
	}

	if (port == 0 || (port & ~0x7)) {
		return -1;
	}

	if ( start % PGSIZE != 0) {
		return -1;
	}

	struct proc *p = curr_proc();

	uint64 size = PGROUNDUP(len);

	int permissions = PTE_U;
	if (port & 1) {
		permissions |= PTE_R;
	}
	if (port & 2) {
		permissions |= PTE_W;
	}
	if (port & 4) {
		permissions |= PTE_X;
	}

	for (uint64 i = 0; i < size; i += PGSIZE) {
		void *pa = kalloc();

		if (pa == 0) {
			return -1;
		}
		memset(pa, 0, PGSIZE);

		if (mappages(p->pagetable, start + i, PGSIZE, (uint64)pa, permissions) != 0) {
			kfree(pa);
			return -1;
		}

	}
	return 0;
}

uint64 sys_munmap(uint64 start, uint64 len) {
	if ( start % PGSIZE != 0) {
		return -1;
	}
	if (len == 0 || len % PGSIZE != 0) {
		return -1;
	}
	struct proc *p = curr_proc();
	uint64 npages = len / PGSIZE;
	uvmunmap(p->pagetable, start, npages, 1);

	sfence_vma();

	return 0;
}
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/

int sys_task_info(TaskInfo *ti) {
	struct proc *p = curr_proc();

	TaskInfo *kern_ti = (TaskInfo *)useraddr(p->pagetable, (uint64)ti);

	for(int i = 0; i < MAX_SYSCALL_NUM; i++) {
		kern_ti->syscall_times[i] = p->syscall_times[i];
	}

	kern_ti->status = Running;
	kern_ti->time = (get_cycle() - p->start_time) / (CPU_FREQ / 1000);

	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	struct proc *p = curr_proc();

	if (id >= 0 && id < MAX_SYSCALL_NUM) {
		p->syscall_times[id]++;
	}

	TaskInfo *ti;
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	case SYS_task_info:
		ti = (TaskInfo *)args[0];
		ret = sys_task_info(ti);
		break;
	case SYS_mmap:
		ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap(args[0], args[1]);
		break;

	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
