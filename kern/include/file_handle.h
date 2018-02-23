#ifndef _FILE_HANDLE_H_
#define _FILE_HANDLE_H_

struct lock;
struct vnode;

struct file_handle {
    struct vnode *fh_file_obj;    /* The actual file object. */
    off_t fh_offset;              /* Offset into the file. */
    int fh_flags;                 /* Specifies the file flags. */
    unsigned fh_num_assoc_procs;  /* Number of associated processes. */
    struct lock *fh_lock;         /* Lock for synchronization. */
};

/* Open a new file per the given flags. */
int fh_create(struct file_handle **fh, char *path, int flags);

/* Reduce the number of processes associated with this file handle and
 * destroy the file handle if no other process is associated with it.
 */
void fh_destroy(struct file_handle *fh);

/* Writes the buffer buf of length buflen to the file. */
int fh_write(struct file_handle *fh, void *buf, size_t buflen, int *size);

/* Reads buflen number of bytes into buf from the file. */
int fh_read(struct file_handle *fh, void *buf, size_t buflen, int *size);

/* Seek to a new position based on pos and whence. */
int fh_lseek(struct file_handle *fh, off_t pos, int whence, off_t *new_pos);

#endif /* _FILE_HANDLE_H_ */

