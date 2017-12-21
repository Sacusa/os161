int sys_open(userptr_t user_filename_ptr, int flags);
int sys_write(int fd, userptr_t user_buf_ptr, size_t buflen);

