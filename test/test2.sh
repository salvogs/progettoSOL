#!/bin/bash

# RED='\033[0;31m'
LBLUE='\033[1;34m'
LGREEN='\033[1;32m'
NC='\033[0m' # No Color

# avvio server in background
echo -e "${LGREEN}==AVVIO SERVER IN BACKGROUND==${NC}"

./bin/server test/config2.txt &

SPID=$!

# piccola attesa per far si che il server sia pronto ad accettare connessioni
sleep 2s 

socket=socket.sk


# src contiene esattamente 10 files; -W di bigFile.bin causa l'espulsione di api.c (capacita' nfile superata)
echo -e "${LBLUE}==CLIENT 1==${NC}"
./bin/client -p -f $socket -w src -W testfile/bigFile.bin -D test/ejectedDir

# causa l'espulsione di tutti i file (in ordine FIFO)
# non vengono inviati dato che espulsione dopo openFile
echo -e "${LBLUE}==CLIENT 2==${NC}"
./bin/client -p -f $socket -W testfile/bigFile1.bin,testfile/img.jpg


# modifico img.jpg (non causa espulsione)
echo -e "${LBLUE}==CLIENT 3==${NC}"
./bin/client -p -f $socket -W testfile/img.jpg


# file troppo grande
echo -e "${LBLUE}==CLIENT 4==${NC}"
./bin/client -p -f $socket -W testfile/verybigFile.bin


# scrivendo 1Mb.bin(che causa espulsione), ricevo img.jpg che è stato modificato
echo -e "${LBLUE}==CLIENT 5==${NC}"
./bin/client -p -f $socket -W testfile/1Mb.bin -D test/ejectedDir

kill -s SIGHUP $SPID
wait $SPID

exit 0