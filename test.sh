#!/bin/sh
expect() {
	addr=$1
	opts=$(echo $2 |tr , '\n' |sort |tr '\n' , |sed 's/,$//')
	mtpt=$(eval 'echo $'$#)
	expected="mount -t 9p -o $opts $addr $mtpt"
	shift; shift
	actual=$(9mount -n "$@" 2>&1)
	aopts=$(echo $actual |sed 's/.*-o \([^ ]*\) .*/\1/' |tr , '\n' |sort |tr '\n' , |sed 's/,$//')
	actual=$(echo $actual |sed 's/-o [^ ]*/-o '"$aopts"'/')
	if [ "$expected" != "$actual" ]; then
		echo '	'9mount "$@"
		echo $expected' #expected'
		echo $actual' #actual'
		exit 1
	fi
}

mtpt=/tmp/9mount
mkdir -p $mtpt
trap 'rmdir $mtpt' EXIT
dfltopts="name=$USER,uname=$USER,noextend,nodev"

expect 127.0.0.1 $dfltopts,tcp,trans=tcp,port=888 tcp!localhost!888 $mtpt
expect /tmp/ns.$USER.:0/foo $dfltopts,unix,trans=unix unix!/tmp/ns.$USER.:0/foo $mtpt
expect /dev/bar $dfltopts,virtio,trans=virtio virtio!/dev/bar $mtpt
expect nodev $dfltopts,fd,trans=fd,rfdno=0,wrfdno=1 - $mtpt

dfltopts="tcp,trans=tcp,$dfltopts"

testflag() {
	opts=$1
	shift
	expect 127.0.0.1 $dfltopts,$opts "$@" tcp!localhost $mtpt
}

testflag "uid=$(id -u),gid=$(id -g),dfltuid=$(id -u),dfltgid=$(id -g)" -i
testflag "access=any" -s
expect 127.0.0.1 $(echo $dfltopts |sed 's/,noextend//') -u tcp!localhost $mtpt
expect 127.0.0.1 $(echo $dfltopts |sed 's/,nodev//') -v tcp!localhost $mtpt
testflag "access=$(id -u)" -x
testflag "aname=abcdef" -a abcdef
testflag "cache=loose" -c loose
testflag "debug=0x0130" -d fcall,conv,mux
testflag "msize=16384" -m 16384

shouldfail() {
	output=$(9mount -n "$@" 2>&1) && {
		echo '	'9mount "$@"
		echo $output' #should have failed!'
	}
}

shouldfail -a tcp!localhost $mtpt
shouldfail -a main/active,bwahaha tcp!localhost $mtpt
shouldfail -d lol tcp!localhost $mtpt
shouldfail -m z tcp!localhost $mtpt
shouldfail udp!localhost $mtpt
shouldfail unix!/tmp/9mount!qux $mtpt
shouldfail virtio!/dev/chan!bar $mtpt
shouldfail tcp!localhost!564!foo $mtpt
