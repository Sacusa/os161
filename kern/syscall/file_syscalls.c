#include <types.h>
#include <limits.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <vm.h>
#include <vfs.h>
#include <copyinout.h>
#include <proc.h>
#include <current.h>
#include <file_handle.h>
#include <file_syscalls.h>

/*
 * If an empty file descriptor is found, its value is put into 'fd' and 0
 * is returned. Otherwise, 'fd' is unchanged and the error code is returned.
 */
int
sys_open(userptr_t user_filename_ptr, int flags, int *fd)
{
    char *path = kmalloc(PATH_MAX * sizeof(char));
    int err = 0;

    /* copy user string into kernel space */
    err = copyinstr(user_filename_ptr, path, PATH_MAX, NULL);
    if (err) {
        return err;
    }

    /* create a new file handle */
    struct file_handle *fh = NULL;
    err = fh_create(&fh, path, flags);
    if (err) {
        return err;
    }

    err = proc_addfile(fh);
    if (err < 0) {
        /*
         * Negative fd indicates inability to add the file.
         * Inability to add file indicates that too many files are open.
         */
        return EMFILE;
    }

    *fd = err;
    kfree(path);
    
    return 0;
}

/*
 * If the write is successful, 0 is returned and the number of bytes written
 * are reflected in 'size'. Otherwise, an error code is returned and
 * 'size' is unchanged.
 */
int
sys_write(int fd, userptr_t user_buf_ptr, size_t buflen, int *size)
{
    /* Make sure the fd is valid */
    if (((unsigned)fd >= curproc->p_ft_size) || (fd < 0)) {
        return EBADF;
    }

    int result;

    /* Copy the buffer to be written into kernel space */
    void *buf = kmalloc(buflen);
    result = copyin(user_buf_ptr, buf, buflen);
    if (result) {
        kfree(buf);
        return result;
    }

    result = fh_write(curproc->p_ft[fd], buf, buflen, size);
    if (result) {
        kfree(buf);
        return result;
    }

    kfree(buf);
    return 0;
}

/*
 * If the read is successful, 0 is returned and the number of bytes read
 * are reflected in 'size'.
 * Otherwise, an error code is returned and 'size' and 'buffer' are
 * unchanged.
 */
int
sys_read(int fd, userptr_t user_buf_ptr, size_t buflen, int *size)
{
    /* Make sure the fd is valid */
    if (((unsigned)fd >= curproc->p_ft_size) || (fd < 0)) {
        return EBADF;
    }

    int result;

    void *buf = kmalloc(buflen);
    result = fh_read(curproc->p_ft[fd], buf, buflen, size);
    if (result) {
        kfree(buf);
        return result;
    }

    /* Copy the read buffer to user space */
    result = copyout(buf, user_buf_ptr, buflen);
    if (result) {
        kfree(buf);
        return result;
    }

    kfree(buf);
    return 0;
}
