prefix=/usr/local
bindir=$(prefix)/bin
mandir=$(prefix)/share/man

CFLAGS=-Wall


all: 9mount 9umount

9mount: 9mount.c
9umount: 9umount.c

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f 9mount 9umount

install: all
	mkdir -p $(bindir)
	cp -f 9mount $(bindir)
	cp -f 9umount $(bindir)
	chown root:users $(bindir)/9mount $(bindir)/9umount
	chmod 4750 $(bindir)/9mount $(bindir)/9umount
