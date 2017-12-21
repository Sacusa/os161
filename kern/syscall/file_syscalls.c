#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <vm.h>
#include <vfs.h>
#include <copyinout.h>
#include <file_syscalls.h>

int
sys_open(userptr_t user_filename_ptr, int flags)
{
    // TODO
    (void) user_filename_ptr;
    (void) flags;
    return 0;
}

int
sys_write(int fd, userptr_t user_buf_ptr, size_t buflen)
{
    // TODO
    (void) fd;
    (void) user_buf_ptr;
    (void) buflen;
    return 0;
}

