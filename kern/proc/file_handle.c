#include <types.h>
#include <file_handle.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/iovec.h>
#include <uio.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <kern/seek.h>

/*
 * Creates a new file handle to the file 'path' and returns the corresponding
 * error code. Options are provided using 'flags', as documented in the man
 * page for open() syscall.
 * 
 * Requires fh to be null. Memory for the new file handle
 * will be allocated automatically.
 * 
 * Returns 0 on success. Error code otherwise.
 */
int
fh_create(struct file_handle **fh, char *path, int flags)
{
    KASSERT(*fh == NULL);
    KASSERT(path != NULL);

    struct file_handle *new_fh;
    int result;

    /* Check flags */
    result = flags & 0x03;
    if (!((result == 0) || (result == 1) || (result == 2))) {
        return EINVAL;
    }

    new_fh = kmalloc(sizeof(struct file_handle));
    if (new_fh == NULL) {
        return ENOMEM;
    }

    new_fh->fh_file_obj = kmalloc(sizeof(struct vnode));
    if (new_fh->fh_file_obj == NULL) {
        kfree(new_fh);
        return ENOMEM;
    }

    result = vfs_open(path, flags, 0664, &(new_fh->fh_file_obj));
    if (result) {
        kfree(new_fh->fh_file_obj);
        kfree(new_fh);
        return result;
    }

    /* Set offset according to flags */
    if (flags & O_APPEND) {
        struct stat *file_info = kmalloc(sizeof(struct stat));

        result = VOP_STAT(new_fh->fh_file_obj, file_info);
        if (result) {
            kfree(new_fh->fh_file_obj);
            kfree(new_fh);
            kfree(file_info);
            return result;
        }

        new_fh->fh_offset = file_info->st_size;
        kfree(file_info);
    }
    else {
        new_fh->fh_offset = 0;
    }

    new_fh->fh_flags = flags & 0x03;
    new_fh->fh_refcount = 1;
    new_fh->fh_lock = lock_create(path);

    *fh = new_fh;

    return 0;
}

/*
 * Reduces the number of processes associated with the file handle. If the
 * count reaches 0, the file handle is destroyed.
 */
void
fh_destroy(struct file_handle *fh)
{
    KASSERT(fh != NULL);

    fh->fh_refcount -= 1;

    if (fh->fh_refcount == 0) {
        vfs_close(fh->fh_file_obj);
        lock_destroy(fh->fh_lock);
        kfree(fh);
    }
}

/*
 * Writes the buffer pointed to by 'buf', of size buflen, to the file 'fh'.
 * The actual number of bytes written are stored in 'size'.
 * 
 * Returns 0 on success, error value otherwise.
 */
int
fh_write(struct file_handle *fh, void *buf, size_t buflen, int *size)
{
    KASSERT(buf != NULL);
    KASSERT(size != NULL);

    /* Make sure we have the permission to write */
    if (fh->fh_flags == O_RDONLY) {
        return EPERM;
    }

    lock_acquire(fh->fh_lock);

    struct iovec iov;
    struct uio uio;
 
    uio_kinit(&iov, &uio, buf, buflen, fh->fh_offset, UIO_WRITE);
    int result = VOP_WRITE(fh->fh_file_obj, &uio);
    if (result) {
        lock_release(fh->fh_lock);
        return result;
    }

    /* Determine the number of bytes written */
    *size = uio.uio_offset - fh->fh_offset;

    /* Update the file offset information */
    fh->fh_offset = uio.uio_offset;

    lock_release(fh->fh_lock);

    return 0;
}

/*
 * Reads 'buflen' number of bytes into the buffer 'buf' from the file 'fh'.
 * The actual number of bytes read are stored in 'size'.
 * 
 * Returns 0 on success, error value otherwise.
 */
int
fh_read(struct file_handle *fh, void *buf, size_t buflen, int *size)
{
    KASSERT(buf != NULL);
    KASSERT(size != NULL);

    /* Make sure the file is not write-only */
    if (fh->fh_flags == O_WRONLY) {
        return EPERM;
    }

    lock_acquire(fh->fh_lock);

    struct iovec iov;
    struct uio uio;
 
    uio_kinit(&iov, &uio, buf, buflen, fh->fh_offset, UIO_READ);
    int result = VOP_READ(fh->fh_file_obj, &uio);
    if (result) {
        lock_release(fh->fh_lock);
        return result;
    }

    /* Determine the number of bytes read */
    *size = uio.uio_offset - fh->fh_offset;

    /* Update the file offset information */
    fh->fh_offset = uio.uio_offset;

    lock_release(fh->fh_lock);

    return 0;
}

/*
 * Seek to a new position based on pos and whence.
 * The new position is stored in 'new_pos'.
 * 
 * Returns 0 on success, error value otherwise.
 */
int
fh_lseek(struct file_handle *fh, off_t pos, int whence, off_t *new_pos)
{
    KASSERT(fh != NULL);

    /* Make sure the file is seekable */
    if (!VOP_ISSEEKABLE(fh->fh_file_obj)) {
        return ESPIPE;
    }

    off_t calc_pos = -1;

    lock_acquire(fh->fh_lock);

    if (whence == SEEK_SET) {
        calc_pos = pos;
    }
    else if (whence == SEEK_CUR) {
        calc_pos = fh->fh_offset + pos;
    }
    else if (whence == SEEK_END) {
        struct stat *file_info = kmalloc(sizeof(struct stat));

        /* Get file info */
        int result = VOP_STAT(fh->fh_file_obj, file_info);
        if (result) {
            kfree(file_info);
            return result;
        }

        calc_pos = file_info->st_size + pos;

        kfree(file_info);
    }

    if (calc_pos < 0) {
        return EINVAL;
    }

    fh->fh_offset = calc_pos;
    *new_pos = calc_pos;

    lock_release(fh->fh_lock);

    return 0;
}

/*
 * Increase the reference count for file handle 'fh'.
 */
void
fh_inc_refcount(struct file_handle *fh)
{
    lock_acquire(fh->fh_lock);
    fh->fh_refcount += 1;
    lock_release(fh->fh_lock);
}
