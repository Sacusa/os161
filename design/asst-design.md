# Design Document

This document describes the various design decisions made when implementing functionalities for ASST2 and ASST3.

## ASST 2

### 1. File Table

The file table is private to each process. It is, however, duplicated whenever fork() is called. Consequently, parent and child processes have the same file table when fork() returns.

The file table has been implemented inside the proc structure as an array of file handles. The following methods are provided to operate on it:

1. __int proc_addfile(struct file_handle *fh)__: This finds an available file descriptor and assigns it to the given file handle. The file descriptor is returned. If the table is full, its size is doubled and the first free index is used. If no index is available (a rare case), -1 is returned.

1. __struct file_handle *proc_remfile(int fd)__: Given a file descriptor, it returns the associated file handle and frees up the file descriptor. The file descriptors are recycled, i.e. they can be reused later for some other file.

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
        unsigned fh_num_assoc_procs;
        struct lock *fh_lock;
    }

The following associated methods are provided:

1. __int fh_create(struct file_handle *fh, char *path, int flags)__: Creates a new file handle to the file 'path' and returns the corresponding error code (0 on success, error value otherwise). Options are provided using 'flags', as documented in the man page for open() syscall.

1. __void fh_destroy(struct file_handle *fh)__: Reduces the number of processes associated with the file handle. If the count reaches 0, the file handle is destroyed.

1. A method for reading. Read using uio (as documented in uio.h). Copy void* (in iovec) data to a userptr using copyout.

1. __int fh_write(struct file_handle *fh, void *buf, size_t buflen, int *size)__: Writes the buffer pointed to by 'buf', of size buflen, to the file 'fh'. The actual number of bytes written are stored in 'size'. Returns 0 on success, error value otherwise.

### 3. Process Table

TODO

### 4. Process Structure

The process structure (as defined in proc.h) has been modified, and now includes the following two fields:

    struct file_handle **p_ft;
    unsigned p_ft_size;

The proc methods have been modified as follows:

1. __proc_create()__: p_ft_size is intitialized to a value of 4. The file table is also, consequently, initialized to a size of 4. File descriptors 0, 1 and 2 are initialized to STDIN, STDOUT and STDERR respectively. File descriptor 3 is initialized to NULL.

2. __proc_destroy()__: fh_destroy() is called on every file handle in the file table. p_ft is then freed.

### 5. Synchronization

TODO

### 6. Syscalls

#### 6.1 File System Support

1. __open__

        int sys_open(userptr_t user_filename_ptr, int flags, int *fd);

    The above function is the kernel-level function that implements the open syscall. The filename is copied into the kernel space using copyinstr. If an empty file descriptor is found, its value is put into 'fd' and 0 is returned. Otherwise, 'fd' is unchanged and the error code is returned.

1. __write__

        int sys_write(int fd, userptr_t user_buf_ptr, size_t buflen, int *size);

    The above function is the kernel-level function that implements the write syscall. The buffer to write is copid into the kernel space using copyin. If the write is successful, 0 is returned and the number of bytes written are reflected in 'size'. Otherwise, an error code is returned and the 'size' may change.

### 7. Exception Handling

TODO
