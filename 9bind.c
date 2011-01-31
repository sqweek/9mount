/* © 2008 sqweek <sqweek@gmail.com>
 * See COPYING for details.
 */
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>

int
main(int argc, char **argv)
{
	char *old = NULL, *new = NULL;
	struct stat stbuf;
	int pid, status;

	while (*++argv) {
		if (!old) {
			old = *argv;
		} else if (!new) {
			new = *argv;
		} else {
			errx(1, "%s: too many arguments", *argv);
		}
	}

	if (!old || !new) {
		errx(1, "usage: 9bind old new");
	}

	/* Make sure mount exists, is writable, and not sticky */
	if (stat(new, &stbuf) || access(new, W_OK)) {
		err(1, "%s", new);
	}
	if (stbuf.st_mode & S_ISVTX) {
		errx(1, "%s: refusing to bind over sticky directory", new);
	}

	switch(pid=fork()) {
		case -1:
			err(1, "fork");
		case 0:
			setuid(0);
			execl("/bin/mount", "mount", "--bind", old, new, (char*)NULL);
			err(1, "execl");
		default:
			wait(&status);
	}

	return WEXITSTATUS(status);
}
