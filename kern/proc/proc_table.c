#include <types.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <proc_table.h>

/*
 * This is the global proc table to hold all the processes.
 */
struct proc_table *global_proc_table;

/*
 * Adds proc 'p' to proc table 'pt' and assigns it a pid of 'pid'.
 */
int
pt_add_proc(struct proc_table *pt, struct proc *p, pid_t *pid)
{
    KASSERT(pt != NULL);
    KASSERT(p != NULL);

    int result;
    pid_t newpid = 0;

    /* Look for an empty slot in 'pt' */
    for (unsigned i = 1; i < pt->pt_size; ++i) {
        if (pt->pt_table[i] == NULL) {
            newpid = i;
            break;
        }
    }

    if (!newpid) {
        /* No empty slot found.
         * Use a pid greater than the table size. pt_set_proc will take care of
         * resizing the table.
         */
        newpid = pt->pt_size;
    }

    result = pt_set_proc(&pt, p, newpid);
    if (result) {
        return result;
    }

    *pid = newpid;
    return 0;
}

/*
 * Helper function for pt_add_proc to help with resizing.
 */
int
pt_set_proc(struct proc_table **pt, struct proc *p, pid_t pid)
{
    KASSERT(pt != NULL);
    KASSERT(*pt != NULL);
    KASSERT(p != NULL);
    KASSERT(pid >= 0);

    if ((unsigned)pid >= (*pt)->pt_size) {
        /* Resize the table.
         * - The table size will be continuously doubled until the new pid can
         *   be accomodated.
         * - The old contents will then be copied into the new one.
         * - Finally, the empty spaces will be explicitly initialized with
         *   NULL.
         */
        struct proc_table *newpt = kmalloc(sizeof(struct proc_table));
        if (newpt == NULL) {
            return ENOMEM;
        }

        /* Determine the table size needed */
        newpt->pt_size = (*pt)->pt_size * 2;
        while ((unsigned)pid >= newpt->pt_size) {
            newpt->pt_size *= 2;
        }

        newpt->pt_table = kmalloc(sizeof(struct proc *) * newpt->pt_size);
        if (newpt->pt_table == NULL) {
            kfree(newpt);
            return ENOMEM;
        }

        /* Initialize the new table */
        for (unsigned i = 0; i < (*pt)->pt_size; ++i) {
            newpt->pt_table[i] = (*pt)->pt_table[i];
        }
        for (unsigned i = (*pt)->pt_size; i < newpt->pt_size; ++i) {
            newpt->pt_table[i] = NULL;
        }

        /* Delete the old table and store the new one */
        kfree((*pt)->pt_table);
        kfree(*pt);
        *pt = newpt;
    }

    (*pt)->pt_table[pid] = p;
    
    return 0;
}

/*
 * Remove the process associated with pid 'pid'.
 */
void
pt_rem_proc(struct proc_table *pt, pid_t pid)
{
    pt->pt_table[pid] = NULL;
}
