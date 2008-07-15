#include <err.h>
#include <mntent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <netdb.h>

char*
append(char **dest, char *src, int *destlen)
{
	while (strlen(*dest) + 1 + strlen(src) > *destlen)
		*destlen *= 2;
	if (!(*dest=realloc(*dest, *destlen)))
		errx(1, "Out of memory");

	if (**dest)
		strcat(*dest, ",");
	strcat(*dest, src);
	return *dest;
}

char*
getarg(char opt, char *cp, char*** argv)
{
	if (*(cp+1)) {
		return cp+1;
	} else if (*(*argv+1)) {
		return *++(*argv);
	} else {
		errx(1, "-%c: expected argument", opt);
	}
	return NULL;
}

void
parsedial(char *dial, char **network, char **netaddr, int *port)
{
	if (!(*network=strtok(dial, "!"))) {
		errx(1, "empty dial string");
	}
	if (strcmp(*network, "unix") != 0 && strcmp(*network, "tcp") != 0) {
		errx(1, "%s: unknown network (expecting unix or tcp)", *network);
	}
	if (!(*netaddr=strtok(NULL, "!"))) {
		errx(1, "missing dial netaddress");
	}
	if (strcmp(*network, "tcp") == 0) {
		char *service;
		if ((service=strtok(NULL, "!"))) {
			if (strspn(service, "0123456789") == strlen(service)) {
				*port = atoi(service);
			} else {
				struct servent *sv;
				if ((sv=getservbyname(service, *network))) {
					/* sv->s_port is a 16-bit big endian masquerading as an int */
					*port = ntohs((uint16_t)sv->s_port);
					endservent();
				} else {
					errx(1, "%s: unknown service", service);
				}
			}
		}
	}
}

int
main(int argc, char **argv)
{
	char buf[256], *opts, *dial = NULL, *mountpt = NULL;
	int optlen = 64, port = 0;
	struct stat stbuf;
	struct passwd *pw;
	int dotu = 0, uidgid = 0, dev = 0, debug = 0, dryrun = 0;
	char *aname = NULL, *proto, *addr;
	/* FILE *fp;
	struct mntent m; */

	if (!(opts=calloc(optlen, 1))) {
		err(1, "calloc");
	}
	while (*++argv) {
		char *cp;

		if (**argv == '-') {
			for (cp=*argv+1; *cp; ++cp) {
				switch (*cp) {
					case 'd': ++debug; break;
					case 'i': uidgid = 1; break;
					case 'n': dryrun = 1; break;
					case 'u': dotu = 1; break;
					case 'v': dev = 1; break;
					case 'a':
						aname = getarg('a', cp, &argv);
						*cp-- = '\0'; /* breaks out of for loop */
						break;
				}
			}
		} else if (!dial) {
			dial = *argv;
		} else if (!mountpt) {
			mountpt = *argv;
		} else {
			errx(1, "%s: too many arguments", *argv);
		}
	}

	if (!dial || !mountpt) {
		errx(1, "usage: 9mount [ -diuv ] [ -a spec ] dial mountpt");
	}

	if (aname && strchr(aname, ',')) {
		errx(1, "spec can't contain commas");
	}

	/* Make sure mount exists, is writable, and not sticky */
	if (stat(mountpt, &stbuf) || access(mountpt, W_OK)) {
		err(1, "%s", mountpt);
	}
	if (stbuf.st_mode & S_ISVTX) {
		errx(1, "%s: refusing to mount over sticky directory", mountpt);
	}

	parsedial(dial, &proto, &addr, &port);

	/* set up mount options */
	append(&opts, proto, &optlen); /* < 2.6.24 */
	snprintf(buf, sizeof(buf), "trans=%s", proto);
	append(&opts, buf, &optlen); /* >= 2.6.24 */

	pw = getpwuid(getuid());
	snprintf(buf, sizeof(buf), "uname=%s", pw->pw_name);
	append(&opts, buf, &optlen);

	if (!dotu) {
		append(&opts, "noextend", &optlen);
	}
	if (!dev) {
		append(&opts, "nodev", &optlen);
	}
	if (uidgid) {
		snprintf(buf, sizeof(buf), "uid=%d,gid=%d", getuid(), getgid());
		append(&opts, buf, &optlen); /* < 2.6.24 */
		snprintf(buf, sizeof(buf), "dfltuid=%d,dfltgid=%d", getuid(), getgid());
		append(&opts, buf, &optlen); /* >= 2.6.24 */
	}
	if (debug) {
		snprintf(buf, sizeof(buf), "debug=%d", debug);
		append(&opts, buf, &optlen);
	}
	if (aname) {
		snprintf(buf, sizeof(buf), "aname=%s", aname);
		append(&opts, buf, &optlen);
	}
	if (port) {
		snprintf(buf, sizeof(buf), "port=%d", port);
		append(&opts, buf, &optlen);
	}

	if (strcmp(proto, "tcp") == 0) {
		struct addrinfo *ai;
		int r;
		if ((r=getaddrinfo(addr, NULL, NULL, &ai))) {
			errx(1, "getaddrinfo: %s", gai_strerror(r));
		}
		if ((r=getnameinfo(ai->ai_addr, ai->ai_addrlen, buf,
						sizeof(buf), NULL, 0, NI_NUMERICHOST))) {
			errx(1, "getnameinfo: %s", gai_strerror(r));
		}
	} else { /* unix socket */
		snprintf(buf, sizeof(buf), "%s", addr);
	}

	if(dryrun) {
		printf("mount -t 9p -o %s %s %s\n", opts, buf, mountpt);
	} else if (mount(buf, mountpt, "9p", 0, (void*)opts)) {
		err(1, "mount");
	}

	/*
	m.mnt_fsname = buf;
	m.mnt_dir = mountpt;
	m.mnt_type = "9p";
	m.mnt_opts = opts;
	m.mnt_freq = 0;
	m.mnt_passno = 0;
	if (!(fp=fopen("/etc/mtab", "a")) || addmntent(fp, &m)) {
		warn("mount succeeded but couldn't add entry to /etc/mtab");
	}*/

	return 0;
}
