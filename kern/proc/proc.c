/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <file_handle.h>
#include <thread.h>
#include <proc_table.h>

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
    struct proc *proc;
    int result;

    proc = kmalloc(sizeof(*proc));
    if (proc == NULL) {
        return NULL;
    }
    proc->p_name = kstrdup(name);
    if (proc->p_name == NULL) {
        kfree(proc);
        return NULL;
    }

    proc->p_numthreads = 0;
    spinlock_init(&proc->p_lock);

    /* VM fields */
    proc->p_addrspace = NULL;

    /* VFS fields */
    proc->p_cwd = NULL;

    /* File Table fields */

    /* 
     * Create a file table for non-kernel processes only.
     * Kernel doesn't need files to access the console.
     */
    if (strcmp(name, "[kernel]")) {
        char *stdin_str = kstrdup("con:");
        char *stdout_str = kstrdup("con:");
        char *stderr_str = kstrdup("con:");

        struct file_handle *fh_stdin = NULL;
        result = fh_create(&fh_stdin, stdin_str, O_RDONLY);
        if (result) {
            kfree(stdin_str);
            kfree(stdout_str);
            kfree(stderr_str);
            kfree(proc->p_name);
            kfree(proc);
            return NULL;
        }

        struct file_handle *fh_stdout = NULL;
        result = fh_create(&fh_stdout, stdout_str, O_WRONLY);
        if (result) {
            kfree(stdin_str);
            kfree(stdout_str);
            kfree(stderr_str);
            kfree(proc->p_name);
            kfree(proc);
            fh_destroy(fh_stdin);
            return NULL;
        }

        struct file_handle *fh_stderr = NULL;
        result = fh_create(&fh_stderr, stderr_str, O_WRONLY);
        if (result) {
            kfree(stdin_str);
            kfree(stdout_str);
            kfree(stderr_str);
            kfree(proc->p_name);
            kfree(proc);
            fh_destroy(fh_stdin);
            fh_destroy(fh_stdout);
            return NULL;
        }

        proc->p_ft_size = 4;
        proc->p_ft = kmalloc(sizeof(struct file_handle *) * proc->p_ft_size);
        if (proc->p_ft == NULL) {
            kfree(stdin_str);
            kfree(stdout_str);
            kfree(stderr_str);
            kfree(proc->p_name);
            kfree(proc);
            fh_destroy(fh_stdin);
            fh_destroy(fh_stdout);
            fh_destroy(fh_stderr);
            return NULL;
        }

        proc->p_ft[0] = fh_stdin;
        proc->p_ft[1] = fh_stdout;
        proc->p_ft[2] = fh_stderr;

        kfree(stdin_str);
        kfree(stdout_str);
        kfree(stderr_str);
    }
    else {
        proc->p_ft_size = 0;
        proc->p_ft = NULL;
    }

    /* Process Table */

    /* User processes are simply added to this table and assigned a pid. */
    if (strcmp(name, "[kernel]")) {
        result = pt_add_proc(global_proc_table, proc, &(proc->p_pid));
        if (result) {
            fh_destroy(proc->p_ft[0]);
            fh_destroy(proc->p_ft[1]);
            fh_destroy(proc->p_ft[2]);
            kfree(proc->p_ft);
            kfree(proc->p_name);
            kfree(proc);
            return NULL;
        }
    }
    /* Kernel process creates the process table and gets pid 1. */
    else {
        global_proc_table = kmalloc(sizeof(struct proc_table));

        global_proc_table->pt_size = 4;
        global_proc_table->pt_table = kmalloc(sizeof(struct proc *) * \
                                              global_proc_table->pt_size);
        
        global_proc_table->pt_table[1] = proc;
        proc->p_pid = 1;
    }

    return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void
proc_destroy(struct proc *proc)
{
    /*
     * You probably want to destroy and null out much of the
     * process (particularly the address space) at exit time if
     * your wait/exit design calls for the process structure to
     * hang around beyond process exit. Some wait/exit designs
     * do, some don't.
     */

    KASSERT(proc != NULL);
    KASSERT(proc != kproc);

    /*
     * We don't take p_lock in here because we must have the only
     * reference to this structure. (Otherwise it would be
     * incorrect to destroy it.)
     */

    /* VFS fields */
    if (proc->p_cwd) {
        VOP_DECREF(proc->p_cwd);
        proc->p_cwd = NULL;
    }

    /* VM fields */
    if (proc->p_addrspace) {
        /*
         * If p is the current process, remove it safely from
         * p_addrspace before destroying it. This makes sure
         * we don't try to activate the address space while
         * it's being destroyed.
         *
         * Also explicitly deactivate, because setting the
         * address space to NULL won't necessarily do that.
         *
         * (When the address space is NULL, it means the
         * process is kernel-only; in that case it is normally
         * ok if the MMU and MMU- related data structures
         * still refer to the address space of the last
         * process that had one. Then you save work if that
         * process is the next one to run, which isn't
         * uncommon. However, here we're going to destroy the
         * address space, so we need to make sure that nothing
         * in the VM system still refers to it.)
         *
         * The call to as_deactivate() must come after we
         * clear the address space, or a timer interrupt might
         * reactivate the old address space again behind our
         * back.
         *
         * If p is not the current process, still remove it
         * from p_addrspace before destroying it as a
         * precaution. Note that if p is not the current
         * process, in order to be here p must either have
         * never run (e.g. cleaning up after fork failed) or
         * have finished running and exited. It is quite
         * incorrect to destroy the proc structure of some
         * random other process while it's still running...
         */
        struct addrspace *as;

        if (proc == curproc) {
            as = proc_setas(NULL);
            as_deactivate();
        }
        else {
            as = proc->p_addrspace;
            proc->p_addrspace = NULL;
        }
        as_destroy(as);
    }

    /* File table fields */
    for (unsigned i = 0; i < proc->p_ft_size; ++i) {
        fh_destroy(proc->p_ft[i]);
        proc->p_ft[i] = NULL;
    }
    kfree(proc->p_ft);

    KASSERT(proc->p_numthreads == 0);
    spinlock_cleanup(&proc->p_lock);

    kfree(proc->p_name);
    kfree(proc);
}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{
    kproc = proc_create("[kernel]");
    if (kproc == NULL) {
        panic("proc_create for kproc failed\n");
    }
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
    struct proc *newproc;

    newproc = proc_create(name);
    if (newproc == NULL) {
        return NULL;
    }

    /* VM fields */

    newproc->p_addrspace = NULL;

    /* VFS fields */

    /*
     * Lock the current process to copy its current directory.
     * (We don't need to lock the new process, though, as we have
     * the only reference to it.)
     */
    spinlock_acquire(&curproc->p_lock);
    if (curproc->p_cwd != NULL) {
        VOP_INCREF(curproc->p_cwd);
        newproc->p_cwd = curproc->p_cwd;
    }
    spinlock_release(&curproc->p_lock);

    return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
    int spl;

    KASSERT(t->t_proc == NULL);

    spinlock_acquire(&proc->p_lock);
    proc->p_numthreads++;
    spinlock_release(&proc->p_lock);

    spl = splhigh();
    t->t_proc = proc;
    splx(spl);

    return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void
proc_remthread(struct thread *t)
{
    struct proc *proc;
    int spl;

    proc = t->t_proc;
    KASSERT(proc != NULL);

    spinlock_acquire(&proc->p_lock);
    KASSERT(proc->p_numthreads > 0);
    proc->p_numthreads--;
    spinlock_release(&proc->p_lock);

    spl = splhigh();
    t->t_proc = NULL;
    splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
    struct addrspace *as;
    struct proc *proc = curproc;

    if (proc == NULL) {
        return NULL;
    }

    spinlock_acquire(&proc->p_lock);
    as = proc->p_addrspace;
    spinlock_release(&proc->p_lock);
    return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
    struct addrspace *oldas;
    struct proc *proc = curproc;

    KASSERT(proc != NULL);

    spinlock_acquire(&proc->p_lock);
    oldas = proc->p_addrspace;
    proc->p_addrspace = newas;
    spinlock_release(&proc->p_lock);
    return oldas;
}

/*
 * Assign an empty file descriptor to 'fh' and return it.
 *
 * If the table is not full, the first available file descriptor is used.
 * If the table is full, its size is doubled and the first empty file
 * descriptor is used.
 * 
 * If no index is available (a rare case), -1 is returned.
 */
int
proc_addfile(struct file_handle *fh)
{
    KASSERT(fh != NULL);

    struct proc *proc = curproc;
    int fd = -1;

    spinlock_acquire(&proc->p_lock);

    /* Look for an empty fd within the table */
    for (unsigned i = 3; i < proc->p_ft_size; ++i) {
        if (proc->p_ft[i] == NULL) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        /*
         * No empty fd found. Assign a fd greater than the table size.
         * proc_setfile() will take care of resizing the table and setting
         * the value.
         */
        fd = proc->p_ft_size;
    }

    int result = proc_setfile(fd, fh);
    if (result) {
        return -1;
    }

    /* Spinlock released by proc_setfile() */

    return fd;
}

/*
 * Release the file descriptor 'fd' and return the associated file handle.
 * The file descriptor is recycled, i.e. it is available for use later.
 */
struct file_handle *
proc_remfile(int fd)
{
    KASSERT(fd >= 3);
    KASSERT((unsigned)fd < curproc->p_ft_size);

    spinlock_acquire(&curproc->p_lock);
    struct file_handle *fh = curproc->p_ft[fd];
    curproc->p_ft[fd] = NULL;
    spinlock_release(&curproc->p_lock);

    return fh;
}

/*
 * Set file descriptor 'fd' to point to file 'fh'.
 * 
 * CAUTION: This function will release the proc spinlock before returning.
 * 
 * Returns 0 on success, error code otherwise.
 */
int
proc_setfile(int fd, struct file_handle *fh)
{
    KASSERT(fh != NULL);
    KASSERT(fd >= 0);

    struct proc *proc = curproc;

    /* 
     * The function may be called from another proc function.
     * So, make sure we don't try to re-acquire the lock.
     * Or bad things will happen!
     */
    if (!spinlock_do_i_hold(&proc->p_lock)) {
        spinlock_acquire(&proc->p_lock);
    }

    if ((unsigned)fd >= proc->p_ft_size) {
        /*
         * fd is greater than the table's size. Resize the table.
         * This involves creating a new table, copying over the contents of the
         * old one into the new one, and then destroying the old one.
         */
        struct file_handle **new_ft;
        unsigned old_size = proc->p_ft_size;

        /* Determine the new size */
        unsigned new_size = proc->p_ft_size * 2;
        while ((unsigned)fd >= new_size) {
            new_size *= 2;
        }

        proc->p_ft_size = new_size;

        new_ft = kmalloc(sizeof(struct file_handle *) * new_size);
        if (new_ft == NULL) {
            return ENOMEM;
        }

        for (unsigned i = 0; i < old_size; ++i) {
            new_ft[i] = proc->p_ft[i];
        }
        for (unsigned i = old_size; i < new_size; ++i) {
            new_ft[i] = NULL;
        }

        kfree(proc->p_ft);
        proc->p_ft = new_ft;
    }

    proc->p_ft[fd] = fh;

    spinlock_release(&curproc->p_lock);

    return 0;
}
