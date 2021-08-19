#!/bin/bash

while true
do	
	i=$RANDOM

	if [[ $((i >= 10 && i <= 20)) -eq 1 ]]; then
	echo "20" >> eco.txt
		#./bin/client -f $socket -t 0 -l "$PWD/testfile/bigFile.bin" -W testfile/50byte.txt,testfile/100byte.txt,testfile/bigFile2.bin -D test/ejectedDir -c "$PWD/testfile/bigFile.bin"
		./bin/client -f $socket -t 0 -w testfile -D test/ejectedFile
	
	fi

	if [[ $((i >= 100 && i <= 200)) -eq 1 ]]; then
	echo "100" >> eco.txt
		#./bin/client -f $socket -t 0 -W testfile/5KB.bin,testfile/10KB.bin -D test/ejectedDir -l "$PWD/testfile/emptyFile.bin" -c "$PWD/testfile/emptyFile.bin"
		./bin/client -f $socket -t 0 -R n=3 -d test/readDir -r "$PWD/testfile/emptyFile.bin","$PWD/testfile/img.jpg" -d test/readDir -l "$PWD/testfile/bigFile.bin" -c "$PWD/testfile/bigFile.bin"
	fi

	if [[ $((i >= 1000 && i <=2000)) -eq 1 ]]; then
	echo "2000" >> eco.txt
		./bin/client -f $socket -t 0 -l "$PWD/testFile/bigFile1.bin" -c "$PWD/testFile/bigFile1.bin" -W testfile/bigFile2.bin,testfile/1KB.bin -u "$PWD/testfile/bigFile2.bin"
	
	fi

done