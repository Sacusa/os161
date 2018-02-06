#include <types.h>
#include <file_handle.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/iovec.h>
#include <uio.h>
#include <kern/errno.h>
#include <kern/fcntl.h>

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

    new_fh = kmalloc(sizeof(struct file_handle));
    if (new_fh == NULL) {
        return ENOMEM;
    }

    new_fh->fh_file_obj = kmalloc(sizeof(struct vnode));
    if (new_fh->fh_file_obj == NULL) {
        return ENOMEM;
    }

    result = vfs_open(path, flags, 0664, &(new_fh->fh_file_obj));
    if (result) {
        return result;
    }

    new_fh->fh_offset = 0;
    new_fh->fh_flags = flags & 0x03;
    new_fh->fh_num_assoc_procs = 1;
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

    fh->fh_num_assoc_procs -= 1;

    if (fh->fh_num_assoc_procs == 0) {
        vfs_close(fh->fh_file_obj);
        lock_destroy(fh->fh_lock);
        kfree(fh);
    }
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
    if (fh->fh_flags & O_WRONLY) {
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
    if (!(fh->fh_flags & O_WRONLY)) {
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
