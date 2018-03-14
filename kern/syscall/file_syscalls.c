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
    else {
        if (curproc->p_ft[fd] == NULL) {
            return EBADF;
        }
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
    else {
        if (curproc->p_ft[fd] == NULL) {
            return EBADF;
        }
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

/*
 * Closes the file assocated with file descriptor 'fd'.
 * 
 * Returns 0 on success, error code otherwise.
 */
int
sys_close(int fd)
{
    /* Make sure the fd is valid */
    if (((unsigned)fd >= curproc->p_ft_size) || (fd < 0)) {
        return EBADF;
    }
    else {
        if (curproc->p_ft[fd] == NULL) {
            return EBADF;
        }
    }

    fh_destroy(curproc->p_ft[fd]);
    curproc->p_ft[fd] = NULL;

    return 0;
}

/*
 * If the seek is successful, 0 is returned and the new position in file is
 * reflected in 'new_pos'.
 * Otherwise, an error code is returned and 'new_pos' is unchanged.
 */
int
sys_lseek(int fd, off_t pos, int whence, off_t *new_pos)
{
    /* Make sure fd is valid */
    if (((unsigned)fd >= curproc->p_ft_size) || (fd < 0)) {
        return EBADF;
    }
    else {
        if (curproc->p_ft[fd] == NULL) {
            return EBADF;
        }
    }

    /* Make sure whence is valid */
    if ((whence != SEEK_SET) && (whence != SEEK_CUR) && (whence != SEEK_END)) {
        return EINVAL;
    }

    int result;

    result = fh_lseek(curproc->p_ft[fd], pos, whence, new_pos);
    if (result) {
        return result;
    }

    return 0;
}

/*
 * If the change is successful, 0 is returned.
 * Otherwise, the error code is returned.
 */
int
sys_dup2(int oldfd, int newfd)
{
    /* Make sure oldfd is valid */
    if (((unsigned)oldfd >= curproc->p_ft_size) || (oldfd < 0)) {
        return EBADF;
    }
    else {
        if (curproc->p_ft[oldfd] == NULL) {
            return EBADF;
        }
    }

    /* Make sure newfd is valid */
    if (newfd < 0) {
        return EBADF;
    }

    /* If newfd = oldfd, nothing to do. */
    if (oldfd != newfd) {
        int result;

        /* If newfd is an open file, close it */
        if ((unsigned)newfd < curproc->p_ft_size) {
            if (curproc->p_ft[newfd] != NULL) {
                result = sys_close(newfd);
                if (result) {
                    return result;
                }
            }
        }

        struct file_handle *fh = curproc->p_ft[oldfd];

        result = proc_setfile(newfd, fh);
        if (result) {
            return result;
        }

        /* Increase reference count for the file handle */
        fh_inc_refcount(fh);
    }

    return 0;
}

/*
 * If successful, 0 is returned. Otherwise, the error code is returned.
 */
int
sys_chdir(userptr_t user_pathname_ptr)
{
    int result;

    /* Copy the pathname into kernel space */
    int pathname_len = sizeof(user_pathname_ptr);
    void *pathname = kmalloc(pathname_len);
    result = copyin(user_pathname_ptr, pathname, pathname_len);
    if (result) {
        kfree(pathname);
        return result;
    }

    result = vfs_chdir((char *)pathname);
    kfree(pathname);
    if (result) {
        return result;
    }

    return 0;
}

/*
 * If successful, 0 is returned, the current working directory is stored in the
 * user buffer and the length of the current working directory is stored in
 * size. Otherwise, the error code is returned and size is unchanged.
 */
int
sys___getcwd(userptr_t user_buf_ptr, size_t buflen, int *size)
{
    struct proc *proc = curproc;
    int result;

    void *cwd = proc->p_cwd->vn_data;
    size_t cwdlen = sizeof(*cwd);
    size_t size_to_copyout = (cwdlen > buflen) ? (buflen) : (cwdlen);

    result = copyout(cwd, user_buf_ptr, size_to_copyout);
    if (result) {
        return result;
    }
    *size = size_to_copyout;

    return 0;
}
