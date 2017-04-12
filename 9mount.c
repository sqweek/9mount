/* © 2008 sqweek <sqweek@gmail.com>
 * See COPYING for details.
 */
#include <err.h>
#include <mntent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <netdb.h>

#define nelem(x) (sizeof(x)/sizeof(*(x)))

enum {
	Maxmsize = 8192,
};

struct {char *mnemonic; int mask;} debug_flags[] = {
	{"err", 0x001},
	{"devel", 0x002},
	{"9p", 0x004},
	{"vfs", 0x008},
	{"conv", 0x010},
	{"mux", 0x020},
	{"trans", 0x040},
	{"alloc", 0x080},
	{"fcall", 0x100}
};

char*
append(char **dest, char *src, int *destlen)
{
	while (strlen(*dest) + 1 + strlen(src) > *destlen)
		*destlen *= 2;
	if (!(*dest=realloc(*dest, *destlen)))
		errx(1, "out of memory");

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
	char *cp;
	if (!(*network=strtok(dial, "!"))) {
		errx(1, "empty dial string");
	}
	if (strcmp(*network, "unix") != 0
	&& strcmp(*network, "tcp") != 0
	&& strcmp(*network, "virtio") != 0) {
		errx(1, "%s: unknown network (expecting unix, tcp or virtio)", *network);
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
	if ((cp=strtok(NULL, "!"))) {
		errx(1, "%s: junk trailing dial string", cp);
	}
}

int
main(int argc, char **argv)
{
	char buf[256], *opts, *dial = NULL, *mountpt = NULL;
	int optlen = 64, port = 0, i;
	struct stat stbuf;
	struct passwd *pw;
	int axess = 0, dotu = 0, uidgid = 0, dev = 0, debug = 0, dryrun = 0;
	char *debugstr = NULL, *msize = NULL, *cache = NULL, *aname = NULL;
	char *cp, *proto, *addr;

	if (!(opts=calloc(optlen, 1))) {
		err(1, "calloc");
	}
	while (*++argv) {
		if (**argv == '-' && (*argv)[1] != '\0') {
			for (cp=*argv+1; *cp; ++cp) {
				switch (*cp) {
					case 'i': uidgid = 1; break;
					case 'n': dryrun = 1; break;
					case 's': axess = -1; break;
					case 'u': dotu = 1; break;
					case 'v': dev = 1; break;
					case 'x': axess = getuid(); break;
					case 'a':
						aname = getarg('a', cp, &argv);
						*cp-- = '\0'; /* breaks out of for loop */
						break;
					case 'c':
						cache = getarg('c', cp, &argv);
						*cp-- = '\0';
						break;
					case 'd':
						debugstr = getarg('d', cp, &argv);
						*cp-- = '\0';
						break;
					case 'm':
						msize = getarg('m', cp, &argv);
						*cp-- = '\0';
						break;
					default:
						errx(1, "unrecognised argument '%c'", *cp);
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
		errx(1, "usage: 9mount [ -insuvx ] [ -a spec ] [ -c cache ] [ -d debug ] [ -m msize ] dial mountpt");
	}

	if(!(pw=getpwuid(getuid()))) {
		err(1, "who are you?? getpwuid failed");
	}
	/* Make sure mount exists, is writable, and not sticky */
	if (stat(mountpt, &stbuf) || access(mountpt, W_OK)) {
		err(1, "%s", mountpt);
	}
	if (stbuf.st_mode & S_ISVTX) {
		errx(1, "%s: refusing to mount over sticky directory", mountpt);
	}

	if (strcmp(dial, "-") == 0) {
		proto = "fd";
		addr = "nodev";
		append(&opts, "rfdno=0,wfdno=1", &optlen);
	} else {
		parsedial(dial, &proto, &addr, &port);
	}

	/* set up mount options */
	append(&opts, proto, &optlen); /* < 2.6.24 */
	snprintf(buf, sizeof(buf), "trans=%s", proto);
	append(&opts, buf, &optlen); /* >= 2.6.24 */

	if (aname) {
		if (strchr(aname, ',')) {
			errx(1, "%s: spec can't contain commas", aname);
		}
		snprintf(buf, sizeof(buf), "aname=%s", aname);
		append(&opts, buf, &optlen);
	}

	if (cache) {
		if (strcmp(cache, "loose") != 0) {
			errx(1, "%s: unknown cache mode (expecting loose)", cache);
		}
		snprintf(buf, sizeof(buf), "cache=%s", cache);
		append(&opts, buf, &optlen);
	}

	if (debugstr) {
		for (cp=strtok(debugstr, ","); cp; cp=strtok(NULL, ",")) {
			for (i=0; i<nelem(debug_flags); ++i) {
				if (strcmp(cp, debug_flags[i].mnemonic) == 0) {
					debug |= debug_flags[i].mask;
					break;
				}
			}
			if (i >= nelem(debug_flags)) {
				errx(1, "%s: unrecognised debug channel", cp);
			}
		}
		snprintf(buf, sizeof(buf), "debug=0x%04x", debug);
		append(&opts, buf, &optlen);
	}

	if (msize) {
		unsigned long nmsize;
		char *end = NULL;
		nmsize = strtoul(msize, &end, 10);
		if (*end || nmsize == 0 || nmsize > INT_MAX) {
			errx(1, "%s: msize must be a positive integer", msize);
		}
		if (pw->pw_uid != 0 && nmsize > Maxmsize) {
			nmsize = Maxmsize;
		}
		snprintf(buf, sizeof(buf), "msize=%lu", nmsize);
		append(&opts, buf, &optlen);
	}

	snprintf(buf, sizeof(buf), "name=%s", pw->pw_name);
	append(&opts, buf, &optlen);

	if (getenv("USER")) {
		snprintf(buf, sizeof(buf), "uname=%s", getenv("USER"));
	} else {
		snprintf(buf, sizeof(buf), "uname=%s", pw->pw_name);
	}
	if (strchr(buf, ',')) {
		errx(1, "%s: username can't contain commas", buf+6);
	}
	append(&opts, buf, &optlen);

	if (axess == -1) {
		append(&opts, "access=any", &optlen);
	} else if (axess) {
		snprintf(buf, sizeof(buf), "access=%d", axess);
		append(&opts, buf, &optlen);
	}
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
	if (port) {
		snprintf(buf, sizeof(buf), "port=%d", port);
		append(&opts, buf, &optlen);
	}

	if (strcmp(proto, "tcp") == 0) {
		struct addrinfo *ai;
		struct addrinfo aihints;
		int r;
		memset(&aihints, 0, sizeof(aihints));
		aihints.ai_family = AF_INET;
		aihints.ai_socktype = SOCK_STREAM;
		if ((r=getaddrinfo(addr, NULL, &aihints, &ai))) {
			errx(1, "getaddrinfo: %s", gai_strerror(r));
		}
		if ((r=getnameinfo(ai->ai_addr, ai->ai_addrlen, buf,
						sizeof(buf), NULL, 0, NI_NUMERICHOST))) {
			errx(1, "getnameinfo: %s", gai_strerror(r));
		}
	} else { /* unix socket, virtio device or fd transport */
		snprintf(buf, sizeof(buf), "%s", addr);
	}

	if(dryrun) {
		fprintf(stderr, "mount -t 9p -o %s %s %s\n", opts, buf, mountpt);
	} else if (mount(buf, mountpt, "9p", 0, (void*)opts)) {
		err(1, "mount");
	}

	return 0;
}
