#ifndef SYSCALL_H
#define SYSCALL_H

#include "proc.h"

void syscall();

int sys_task_info(TaskInfo *ti);
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd);
uint64 sys_munmap(uint64 start, uint64 len);

#endif // SYSCALL_H
