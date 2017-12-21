# Design Document

## ASST 2

### 1. File Table

The file table has been implemented inside the proc structure as an array of file handles. The following methods are provided to operate on it:

1. __int get_new_fd(void)__: This returns an available file descriptor. If none is available (a rare case), -1 is returned.

2. __struct file_handle *get_file_handle(int fd)__: Given a file descriptor, it returns the associated file handle. If no associated file handle is found, NULL is returned.

3. __void close_fd(int fd)__: Given a file descriptor, it closes any associated file with it. The file descriptors are recycled, i.e. they may be reused later for some other file.

### 2. File Handle

The file handle abstracts away the actual file object. It uses the following data members to accomplish this:

1. A __vnode__ object for the actual file object.
1. A __uio__ object to store the offset information.

The file handle structure, thus, looks like:

    struct file_handle {
        bool is_readable;
        bool is_writable;
        // something for associated processes
        // something for synchronization
        struct vnode *file_obj;
        struct uio *file_uio;
    }

### 3. Process Table

TODO

### 4. Process Structure

TODO

### 5. Synchronization

TODO

### 6. Syscalls

TODO

### 7. Exception Handling

TODO
