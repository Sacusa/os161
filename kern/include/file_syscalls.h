#ifndef _FILE_SYSCALLS_H_
#define _FILE_SYSCALLS_H_

int sys_open(userptr_t user_filename_ptr, int flags, int *fd);
int sys_write(int fd, userptr_t user_buf_ptr, size_t buflen, int *size);
int sys_read(int fd, userptr_t user_buf_ptr, size_t buflen, int *size);
int sys_close(int fd);
int sys_lseek(int fd, off_t pos, int whence, off_t *new_pos);

#endif /* _FILE_SYSCALLS_H_ */
