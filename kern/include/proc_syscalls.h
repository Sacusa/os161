#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

int sys_fork(struct trapframe *tf, pid_t *pid);
int sys_getpid(pid_t *pid);
int sys_waitpid(pid_t pid, int *status, int options, pid_t *ret_pid);

#endif /* _PROC_SYSCALLS_H_ */