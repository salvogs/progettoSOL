#!/bin/bash

while true
do
	if !(($1 % 2)); then
	./bin/client -f $socket -t 0 -W src/api.c,src/client.c,src/clientParser.c -D ejectedDir -l "$PWD/testfile/bigFile.bin" -c "$PWD/testfile/bigFile.bin"
	./bin/client -f $socket -t 0 -w include -D ejectedDir
	else
	./bin/client -f $socket -t 0 -l "$PWD/src/server.c" -W testfile/bigFile.bin,testfile/img.jpg,testfile/smallFile.bin,testfile/verybigFile.bin -D ejectedDir
	./bin/client -f $socket -t 0 -R n=1 -R n=2 -W testfile/bigFile1.bin -u "$PWD/testfile/bigFile.bin" -r "$PWD/testfile/bigFile.bin"
	./bin/client -f $socket -t 0 -R n=3 -d readDir -r "$PWD/src/client.c","$PWD/src/clientParser.c","$PWD/src/smallFile.bin" -d readDir -l "$PWD/src/verybigFile.bin"
	fi
done