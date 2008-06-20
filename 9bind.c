#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mount.h>

int
main(int argc, char **argv)
{
	char *old = NULL, *new = NULL;
	struct stat stbuf;

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

	if (mount(old, new, NULL, MS_BIND, NULL)) {
		err(1, "mount");
	}

	return 0;
}
