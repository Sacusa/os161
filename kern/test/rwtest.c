/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

void reader_thread(void *, unsigned long);
void writer_thread(void *, unsigned long);

volatile unsigned shared_value = 0;
struct rwlock *shared_lock;

/*
 * Use these stubs to test your reader-writer locks.
 */

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;
	int result;

	unsigned NUM_THREADS = 20;  /* Must be a multiple of 5 */
	unsigned reader_status[NUM_THREADS];
	unsigned writer_status[NUM_THREADS];

	shared_lock = rwlock_create("shared_rwlock");

	kprintf_n("Starting rwt1...\n");

	for (unsigned i = 0; i < NUM_THREADS; ++i) {
		reader_status[i] = 0;
		writer_status[i] = 0;
	}

	/* Create threads in batches of 5 */
	for (unsigned j = 0; j < (NUM_THREADS/5); ++j) {
		for (unsigned i = j * 5; i < ((j * 5) + 5); ++i) {
			result = thread_fork("reader_thread", NULL, reader_thread, (unsigned *)reader_status, i);
			if (result) {
				panic("rwt1: thread_fork failed: %s\n", strerror(result));
			}

			result = thread_fork("writer_thread", NULL, writer_thread, (unsigned *)writer_status, i);
			if (result) {
				panic("rwt1: thread_fork failed: %s\n", strerror(result));
			}
		}
	}

	bool readers_complete = false;
	bool writers_complete = false;
	kprintf_n("If this hangs, it's broken:\n");

	/* Verify all threads have exited */
	while (!(readers_complete && writers_complete)) {
		readers_complete = true;
		writers_complete = true;

		for (unsigned i = 0; i < NUM_THREADS; ++i) {
			if (reader_status[i] == 0) {
				readers_complete = false;
			}
			if (writer_status[i] == 0) {
				writers_complete = false;
			}
		}
	}

	/* Check final value of shared_value */
	if (shared_value == NUM_THREADS) {
		success(TEST161_SUCCESS, SECRET, "rwt1");
	}
	else {
		success(TEST161_FAIL, SECRET, "rwt1");
	}

	rwlock_destroy(shared_lock);

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;
	shared_value = 0;

	kprintf_n("Starting rwt2...\n");
	
	struct rwlock *rw = rwlock_create("test_rwlock");
	if (rw == NULL) {
		panic("rwt2: rwlock_create failed\n");
	}

	/* Test writer lock */
	rwlock_acquire_write(rw);
	shared_value = 100;
	rwlock_release_write(rw);

	/* Test reader lock */
	rwlock_acquire_read(rw);
	if (shared_value == 100) {
		success(TEST161_SUCCESS, SECRET, "rwt2");
	}
	else {
		success(TEST161_FAIL, SECRET, "rwt2");
	}
	rwlock_release_read(rw);

	rwlock_destroy(rw);

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("Starting rwt3...\n");
	
	struct rwlock *rw = rwlock_create("test_rwlock");
	if (rw == NULL) {
		panic("rwt3: rwlock_create failed\n");
	}

	rwlock_acquire_write(rw);

	secprintf(SECRET, "Should panic...", "rwt3");
	rwlock_release_write(rw);
	rwlock_release_write(rw);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt3");

	/* Don't do anything that could panic. */

	rwlock_destroy(rw);
	rw = NULL;

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("Starting rwt4...\n");
	
	struct rwlock *rw = rwlock_create("test_rwlock");
	if (rw == NULL) {
		panic("rwt4: rwlock_create failed\n");
	}

	rwlock_acquire_read(rw);

	secprintf(SECRET, "Should panic...", "rwt4");
	rwlock_release_write(rw);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt4");

	/* Don't do anything that could panic. */

	rwlock_destroy(rw);
	rw = NULL;

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("Starting rwt5...\n");
	
	struct rwlock *rw = rwlock_create("test_rwlock");
	if (rw == NULL) {
		panic("rwt5: rwlock_create failed\n");
	}

	secprintf(SECRET, "Should panic...", "rwt5");
	rwlock_release_write(rw);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt5");

	/* Don't do anything that could panic. */

	rwlock_destroy(rw);
	rw = NULL;

	return 0;
}

/*
 * Helper functions below.
 */

void reader_thread(void *reader_status, unsigned long arg2) {
	/* Acquire lock */
	rwlock_acquire_read(shared_lock);

	/* Yield runtime */
	thread_yield();

	/* Print shared value */
	kprintf_n("Reader Value = %d\n", shared_value);

	/* Yield runtime */
	thread_yield();

	/* Release lock */
	rwlock_release_read(shared_lock);

	/* Update exit status */
	((unsigned *)reader_status)[arg2] = 1;
}

void writer_thread(void *writer_status, unsigned long arg2) {
	/* Acquire lock */
	rwlock_acquire_write(shared_lock);

	/* Yield runtime */
	thread_yield();

	/* Increment shared value */
	shared_value += 1;
	kprintf_n("Writer Value = %d\n", shared_value);

	/* Yield runtime */
	thread_yield();

	/* Release lock */
	rwlock_release_write(shared_lock);

	/* Update exit status */
	((unsigned *)writer_status)[arg2] = 1;
}
