#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include <sys/types.h>
#include <sys/mount.h>
#include <mntent.h>
#include <pwd.h>

char*
canonpath(char *rel)
{
	char *abs, *cp, *prev;

	if (rel[0] == '/') {
		if (!(abs=malloc(strlen(rel) + 2))) {
			return NULL;
		}
		abs[0] = '\0';
		strcat(abs, rel);
		strcat(abs, "/");
	} else {
		char *cwd = NULL;
		if (!(cwd=getcwd(NULL, 0))) {
			return NULL;
		}
		if (!(abs=malloc(strlen(cwd) + 1 + strlen(rel) + 2))) {
			free(cwd);
			return NULL;
		}
		abs[0] = '\0';
		strcat(abs, cwd);
		strcat(abs, "/");
		strcat(abs, rel);
		strcat(abs, "/");
		free(cwd);
	}

	/* abs is now an absolute path which begins and ends with a slash */
	prev = abs;
	while ((cp=strchr(prev+1, '/'))) {
		if (cp == prev+1) {
			/* remove consecutive slashes */ 
			memmove(prev, cp, strlen(cp)+1);
		} else if (strncmp(prev, "/./", cp-prev+1) == 0) {
			memmove(prev, cp, strlen(cp)+1);
		} else if (strncmp(prev, "/../", cp-prev+1) == 0) {
			char *parent;
			if (prev == abs) {
				parent = abs; /* just eat .. in root dir */
			} else {
				for (parent=prev-1; *parent != '/'; --parent);
			}
			memmove(parent, cp, strlen(cp)+1);
			prev = parent;
		} else {
			prev = cp;
		}
	}
	abs[strlen(abs)-1] = '\0'; /* remove trailing slash */

	return abs;
}

int
mountedby(char *opts, char *username)
{
	char *s, *cp;
	if (!(s=strdup(opts))) {
		errx(1, "out of memory");
	}
	for (cp=strtok(s, ","); cp; cp=strtok(NULL, ",")) {
		if (strncmp(cp, "name=", 5) == 0) {
			int eq = (strcmp(cp+5, username) == 0);
			free(s);
			return eq;
		}
	}
	free(s);
	return 0;
}

int
main(int argc, char **argv)
{
	int ret = 0;
	char *path, buf[256];
	FILE *fp;
	struct mntent *mnt;
	struct passwd *pw;

	pw = getpwuid(getuid());
	snprintf(buf, sizeof(buf), "uname=%s", pw->pw_name);

	while (*++argv) {
		if (!(path=canonpath(*argv))) {
			warn("%s: canonpath", *argv);
			continue;
		}
		if (!(fp=fopen("/proc/mounts", "r"))) {
			err(1, "couldn't open /proc/mounts");
		}
		while ((mnt=getmntent(fp))) {
			if (strcmp(path, mnt->mnt_dir) == 0) {
				if (strcmp(mnt->mnt_type, "9p")) {
					warnx("%s: refusing to unmount non-9p fs", path);
					ret = 1;
				} else if (!mountedby(mnt->mnt_opts, pw->pw_name)) {
					warnx("%s: not mounted by you", path);
					ret = 1;
				} else if (umount(mnt->mnt_dir)) {
					warn("umount %s", mnt->mnt_dir);
					ret = 1;
				}
				goto done;
			}
		}
		warnx("%s not found in /proc/mounts", path);
		ret = 1;

done:
		free(path);
		fclose(fp);
	}
	return ret;
}
