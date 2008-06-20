prefix=/usr/local
bindir=$(prefix)/bin
mandir=$(prefix)/share/man

CFLAGS=-Wall


all: 9mount 9umount 9bind

9mount: 9mount.c
9umount: 9umount.c
9bind: 9bind.c

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f 9mount 9umount 9bind

install: all
	mkdir -p $(bindir)
	cp -f 9mount $(bindir)
	cp -f 9umount $(bindir)
	cp -f 9bind $(bindir)
	chown root:users $(bindir)/9mount $(bindir)/9umount $(bindir)/9bind
	chmod 4750 $(bindir)/9mount $(bindir)/9umount $(bindir)/9bind
	mkdir -p $(mandir)/man1
	cp -f 9mount.1 $(mandir)/man1
