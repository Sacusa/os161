#ifndef _PROC_TABLE_H_
#define _PROC_TABLE_H_

#include <types.h>

struct proc;

struct proc_table {
    struct proc **pt_table;  // array of processes, index using pids
    unsigned pt_size;        // current size of the table
};

/* This is the global proc table to hold all the processes. */
extern struct proc_table *global_proc_table;

/* Add process 'p' to the table and store its pid in 'pid'. */
int pt_add_proc(struct proc_table *pt, struct proc *p, pid_t *pid);

/* Helper function for pt_add_proc. */
int pt_set_proc(struct proc_table **pt, struct proc *p, pid_t pid);

/* Remove the process associated with process id 'pid' from the table. */
void pt_rem_proc(struct proc_table *pt, pid_t pid);

#endif /* _PROC_TABLE_H_ */
