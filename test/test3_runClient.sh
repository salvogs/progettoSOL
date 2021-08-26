#!/bin/bash

while true
do  
  i=$RANDOM

  
  if [[  $((i % 5)) -eq 0 ]]; then
    ./bin/client -f $socket -t 0 -w testfile -D test/ejectedDir -u $PWD/testfile/bigFile1.bin
  fi

  ./bin/client -f $socket -t 0 -R n=3 -d test/readDir -r "$PWD/testfile/emptyFile.bin","$PWD/testfile/img.jpg" -d test/readDir -l "$PWD/testfile/bigFile.bin" -c "$PWD/testfile/bigFile.bin"
  

  ./bin/client -f $socket -t 0 -w testfile/10byte.txt,testfile/50byte.txt -c "$PWD/testFile/10byte.txt","$PWD/testFile/50byte.txt","$PWD/testfile/bigFile.bin"
  
  sleep 0.2
  
done