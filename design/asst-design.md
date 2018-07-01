# Design Document

This document describes the various design decisions made when implementing functionalities for ASST2 and ASST3.

## ASST 2

### 1. File Table

The file table is private to each process. It is, however, duplicated whenever fork() is called. Consequently, parent and child processes have the same file table when fork() returns.

The file table has been implemented inside the proc structure as an array of file handles. The following methods are provided to operate on it:

1. __int proc_addfile(struct file_handle *fh)__: This finds an available file descriptor and assigns it to the given file handle. The file descriptor is returned. proc_setfile() is used for actually setting the value in the file table. If no index is available (a rare case), -1 is returned.

1. __struct file_handle *proc_remfile(int fd)__: Given a file descriptor, it returns the associated file handle and frees up the file descriptor. The file descriptors are recycled, i.e. they can be reused later for some other file.

1. __int proc_setfile(int fd, struct file_handle *fh)__: Sets the file descriptor 'fd' to point to file 'fh'. If fd is beyond the current table's size, table's size is doubled until it becomes greater than 'fd', after which 'fd' is set. Returns 0 on success, error code otherwise.

### 2. File Handle

The file handle abstracts away actual files. It uses the following data members to accomplish this:

1. A vnode object for the actual file object.
1. An off_t object to store the offset information.
1. An integer to store the mode the file is opened in.
1. Number of processes associated with this file handle.
1. A sleeplock for synchronization.

The file handle structure, thus, looks like:

    struct file_handle {
        struct vnode *fh_file_obj;
        off_t fh_offset;
        int fh_flags;
        unsigned fh_refcount;
        struct lock *fh_lock;
    }

The following associated methods are provided:

1. __int fh_create(struct file_handle *fh, char *path, int flags)__: Creates a new file handle to the file 'path' and returns the corresponding error code (0 on success, error value otherwise). Options are provided using 'flags', as documented in the man page for open() syscall.

1. __void fh_destroy(struct file_handle *fh)__: Reduces the number of processes associated with the file handle. If the count reaches 0, the file handle is destroyed.

1. __int fh_write(struct file_handle *fh, void *buf, size_t buflen, int *size)__: Writes the buffer pointed to by 'buf', of size 'buflen', to the file 'fh'. The actual number of bytes written are stored in 'size'. Returns 0 on success, error value otherwise.

1. __int fh_read(struct file_handle *fh, void *buf, size_t buflen, int *size)__: Reads 'buflen' number of bytes into the buffer 'buf' from the file 'fh'. The actual number of bytes read are stored in 'size'. Returns 0 on success, error value otherwise.

1. __void fh_inc_refcount(struct file_handle *fh)__: Increases the reference count for file handle 'fh'. This method does not fail.

### 3. Process Table

Addition of support for user processes necessitated the creation of a process table. The following structure has been created for this purpose:

    struct proc_table {
        struct proc **pt_table;
        unsigned pt_size;
    }

The following associated methods are provided:

1. __pid_t pt_add_proc(struct proc *p)__: Adds process 'p' to the process table and returns a process id. Returns 0 on failure. If the size of process table is full, the size is doubled and the first free pid is returned.

1. __void pt_rem_proc(pid_t pid)__: Removes the process associated with process id 'pid' from the process table and notifies all the processes waiting on it.

### 4. Process Structure

The process structure (as defined in proc.h) has been modified, and now includes the following fields:

    struct file_handle **p_ft;  // process file table
    unsigned p_ft_size;         // process file table size
    pid_t p_pid;                // process id

The proc methods have been modified as follows:

1. __proc_create()__:
    * p_ft_size is intitialized to a value of 4. The file table is also, consequently, initialized to a size of 4. File descriptors 0, 1 and 2 are initialized to STDIN, STDOUT and STDERR respectively. File descriptor 3 is initialized to NULL.
    * The process is added to the process table using *pt_add_proc()*, with the returned pid stored in p_pid.

2. __proc_destroy()__:
    * fh_destroy() is called on every file handle in the file table. p_ft is then freed.
    * *pt_rem_proc()* is called to remove the process from the process table and notify any processes waiting on it.

### 5. Synchronization

TODO

### 6. Syscalls

#### 6.1 File System Support

1. __open__

        int sys_open(userptr_t user_filename_ptr, int flags, int *fd);

    The above function is the kernel-level function that implements the open syscall. The filename is copied into the kernel space using copyinstr. If an empty file descriptor is found, its value is put into 'fd' and 0 is returned. Otherwise, 'fd' is unchanged and the error code is returned.

1. __write__

        int sys_write(int fd, userptr_t user_buf_ptr, size_t buflen, int *size);

    The above function is the kernel-level function that implements the write syscall. The buffer to write is copied into the kernel space using copyin. If the write is successful, 0 is returned and the number of bytes written are reflected in 'size'. Otherwise, an error code is returned and 'size' is unchanged.

1. __read__

        int sys_read(int fd, userptr_t user_buf_ptr, size_t buflen, int *size);

    The above function is the kernel-level function that implements the read syscall. If the read is successful, 0 is returned, the number of bytes read are reflected in 'size', and the read data is copied into user space using copyout. Otherwise, an error code is returned and 'size' and user buffer are unchanged.

1. __lseek__

        int sys_lseek(int fd, off_t pos, int whence, off_t *new_pos);

    The above function is the kernel-level function that implements the lseek syscall. If the seek is successful, 0 is returned and the new position in file is reflected in 'new_pos'. Otherwise, an error code is returned and 'new_pos' is unchanged.

1. __dup2__

        int sys_dup2(int oldfd, int newfd);

    The above function is the kernel-level function that implements the dup2 syscall. If the change is successful, 0 is returned. Otherwise, the error code is returned.

1. __chdir__

        int sys_chdir(userptr_t user_pathname_ptr);

    The above function is the kernel-level function that implements the chdir syscall. If successful, 0 is returned. Otherwise, the error code is returned.

1. __\_\_getcwd__

        int sys___getcwd(userptr_t user_buf_ptr, size_t buflen, int *size);

    The above function is the kernel-level function that implements the __getcwd syscall. If successful, 0 is returned, the current working directory is stored in the user buffer and the length of the current working directory is stored in size. Otherwise, the error code is returned and size is unchanged.

### 7. Exception Handling

TODO
