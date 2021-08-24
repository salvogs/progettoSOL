#!/bin/bash

while true
do	
	i=$RANDOM

	if [[ $((i >= 10 && i <= 20)) -eq 1 ]]; then
		./bin/client -f $socket -t 0 -w testfile -D test/ejectedDir
	fi

	if [[ $((i >= 100 && i <= 200)) -eq 1 ]]; then
		./bin/client -f $socket -t 0 -R n=3 -d test/readDir -r "$PWD/testfile/emptyFile.bin","$PWD/testfile/img.jpg" -d test/readDir -l "$PWD/testfile/bigFile.bin" -c "$PWD/testfile/bigFile.bin"
	fi

	if [[ $((i >= 1000 && i <=2000)) -eq 1 ]]; then
		./bin/client -f $socket -t 0 -l "$PWD/testFile/bigFile1.bin" -c "$PWD/testFile/bigFile1.bin" -W testfile/bigFile2.bin,testfile/1KB.bin -u "$PWD/testfile/bigFile2.bin"
	fi

	if [[ $((i >= 10000 && i <=11000)) -eq 1 ]]; then
		./bin/client -f $socket -t 0 -w testfile/10byte.txt,testfile/50byte.txt -c "$PWD/testFile/10byte.txt","$PWD/testFile/50byte.txt"
	fi

	
done