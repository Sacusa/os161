#include <types.h>
#include <limits.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#include <vm.h>
#include <vfs.h>
#include <vnode.h>
#include <copyinout.h>
#include <proc.h>
#include <addrspace.h>
#include <current.h>
#include <synch.h>
#include <syscall.h>
#include <file_handle.h>
#include <proc_syscalls.h>

int sys_fork(struct trapframe *tf, pid_t *pid)
{
    struct proc *child_proc = proc_create_runprogram(kstrdup("USER"));
    if (child_proc == NULL) {
        *pid = 0;
        return ENOMEM;
    }
    *pid = child_proc->p_pid;

    // DEBUG
    // kprintf("FORK: new process created with pid = %d.\n", *pid);

    int result;

    /* Copy address space */
    if (curproc->p_addrspace != NULL) {
        result = as_copy(curproc->p_addrspace, &(child_proc->p_addrspace));
        if (result) {
            proc_destroy(child_proc);
            return result;
        }
    }

    // DEBUG
    // kprintf("FORK: address space copied.\n");

    /* Clear the default file table allocation. */
    fh_destroy(child_proc->p_ft[0]);
    fh_destroy(child_proc->p_ft[1]);
    fh_destroy(child_proc->p_ft[2]);
    kfree(child_proc->p_ft);

    spinlock_acquire(&curproc->p_lock);

    /* Copy file table */
    child_proc->p_ft_size = curproc->p_ft_size;
    child_proc->p_ft = kmalloc(sizeof(struct file_handle *) * \
                            child_proc->p_ft_size);
    if (child_proc->p_ft == NULL) {
        proc_destroy(child_proc);
        return ENOMEM;
    }
    
    for (unsigned i = 0; i < child_proc->p_ft_size; ++i) {
        child_proc->p_ft[i] = curproc->p_ft[i];
        if (curproc->p_ft[i] != NULL) {
            fh_inc_refcount(curproc->p_ft[i]);
        }
    }

    // DEBUG
    // kprintf("FORK: file table copied.\n");

    spinlock_release(&curproc->p_lock);

    result = thread_fork(kstrdup("USER_THREAD"), child_proc, &enter_forked_process, tf, 2);
    if (result) {
        return result;
    }

    // DEBUG
    // kprintf("FORK successful. Child pid = %d\n", *pid);

    return 0;
}

int sys_getpid(pid_t *pid)
{
    *pid = curproc->p_pid;
    return 0;
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t *ret_pid)
{
    (void) pid;
    (void) status;
    (void) options;
    *ret_pid = 0;
    return 0;
}
