#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include <test161/test161.h>

#define BUFFER_COUNT 128
#define BUFFER_SIZE 128

static int writebuf[BUFFER_SIZE];
static int readbuf[BUFFER_SIZE];

int
main(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	int i, j;
	int fh, fh2, len, ret;
	off_t pos;

	const char * filename = "fileonlytest.dat";

	/*
	 * Test open
	 */

	tprintf("Opening %s\n", filename);

	fh = open(filename, O_RDWR|O_CREAT|O_TRUNC);
	if (fh < 0) {
		err(1, "create failed");
	}

	/*
	 * Test write
	 */

	tprintf("Writing %d bytes.\n", BUFFER_SIZE * BUFFER_COUNT);

	for (i = 0; i < BUFFER_COUNT; i++) {
		for (j = 0; j < BUFFER_SIZE; j++) {
			writebuf[j] = i * j;
		}
		len = write(fh, writebuf, sizeof(writebuf));
		if (len != sizeof(writebuf)) {
			err(1, "write failed");
		}
	}

	/*
	 * Test dup2
	 */
	fh2 = dup2(fh, 100);

	// Seek to file start
	pos = lseek(fh2, 0, SEEK_SET);
	if (pos != 0) {
		err(1, "lseek failed: expected %ld, got %ld", 0, pos);
	}

	/*
	 * Test read
	 */

	tprintf("Verifying write.\n");

	for (i = 0; i < BUFFER_COUNT; i++) {
		len = read(fh2, readbuf, sizeof(readbuf));
		if (len != sizeof(readbuf)) {
			err(1, "read failed");
		}
		for (j = 0; j < BUFFER_SIZE; j++) {
			if (readbuf[j] != i * j) {
				err(1, "read mismatch: readbuf[j]=%d, i*j=%d, i=%d, j=%d", readbuf[j], i * j, i, j);
			}
		}
	}

	/*
	 * Test cwd
	 */

	size_t cwdlen = 1024;
	char cwd[cwdlen];
	tprintf("cwd: %s\n", getcwd(cwd, cwdlen));

	/*
	 * Test close
	 */

	ret = close(fh);
	if (ret) {
		err(1, "Failed to close file using first file handle\n");
	}

	ret = close(fh2);
	if (ret) {
		err(1, "Failed to close file using second file handle\n");
	}

	ret = close(fh);
	if (!ret) {
		err(1, "File still open after close\n");
	}

	ret = close(fh2);
	if (!ret) {
		err(1, "File still open after close\n");
	}

	/*
	 * Test fork
	 */

	pid_t pid = fork();

	if (pid == 0) {
		tprintf("Inside child process. My pid = %d.\n", getpid());
	}
	else {
		tprintf("Inside parent process. My pid = %d. Child pid = %d.\n", getpid(), pid);
	}
  
	success(TEST161_SUCCESS, SECRET, "/testbin/mytest");
    return 0;
}
