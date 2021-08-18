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
./bin/client -p -f $socket -w src -W testfile/bigFile.bin -D ejectedDir
echo -e "${LBLUE}==CLIENT 1 DISCONNESSO==${NC}"

# -W di bigFile1.bin causa l'espulsione di tutti i file (in ordine FIFO)
./bin/client -p -f $socket -W testfile/bigFile1.bin -D ejectedDir 
echo -e "${LBLUE}==CLIENT 2 DISCONNESSO==${NC}"


# append di bigFile1.bin NON causa l'espulsione (non espello lo stesso file che devo appendere)
./bin/client -p -f $socket -W testfile/bigFile1.bin
echo -e "${LBLUE}==CLIENT 3 DISCONNESSO==${NC}"


# file troppo grande
./bin/client -p -f $socket -W testfile/veryBigFile.bin
echo -e "${LBLUE}==CLIENT 4 DISCONNESSO==${NC}"

kill -s SIGHUP $SPID

wait $SPID

exit 0