#!/bin/bash

# RED='\033[0;31m'
LBLUE='\033[1;34m'
LGREEN='\033[1;32m'
NC='\033[0m' # No Color

# avvio server in background
echo -e "${LGREEN}==AVVIO SERVER IN BACKGROUND==${NC}"

valgrind --leak-check=full ./bin/server test/config1.txt &

SPID=$!

# piccola attesa per far si che il server sia pronto ad accettare connessioni
sleep 2s 

socket=socket.sk
reqTime=200


echo -e "${LBLUE}==CLIENT 1==${NC}"
./bin/client -p -f $socket -t $reqTime -w src -w include,n=3 -r "$PWD/src/server.c","$PWD/src/client.c" -d test/readDir 


echo -e "${LBLUE}==CLIENT 2==${NC}"
./bin/client -p -f $socket -t $reqTime -W testfile/verybigFile.bin,testfile/img.jpg,testfile/smallFile.bin




echo -e "${LBLUE}==AVVIO CLIENT 3/4 IN BACKGROUND==${NC}"
./bin/client -p -f $socket -t $reqTime -l "$PWD/src/client.c","$PWD/src/server.c" -W src/server.c -c "$PWD/src/client.c" -u "$PWD/src/server.c" -R n=0 -d test/readDir &
client3=$!


./bin/client -p -f $socket -t $reqTime -W src/api.c -l "$PWD/src/server.c" -c "$PWD/src/server.c","$PWD/src/client.c" &
client4=$!

wait $client3
echo -e "${LBLUE}==CLIENT 3 DISCONNESSO==($client3)${NC}"
wait $client4
echo -e "${LBLUE}==CLIENT 4 DISCONNESSO==($client4)${NC}"



kill -s SIGHUP $SPID

wait $SPID

exit 0