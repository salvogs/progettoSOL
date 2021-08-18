#!/bin/bash

# RED='\033[0;31m'
LBLUE='\033[1;34m'
LGREEN='\033[1;32m'
NC='\033[0m' # No Color

export socket=socket.sk

# avvio server in background
echo -e "${LGREEN}==AVVIO SERVER IN BACKGROUND==${NC}"
valgrind --leak-check=full -q ./bin/server test/config3.txt &

sleep 3s
SPID=$!



for i in {1..10}
do
	./test/test3_runClient.sh $i &
done

s=30
while [ $s -gt 0 ]
do
	echo -n -e "Attendi ${LBLUE}$s${NC} secondi.\r"
	sleep 1s
	s=$(( $s - 1 ))
done 


kill -s SIGINT $SPID 
wait $SPID
killall -s SIGKILL test3_runClient.sh client 
exit 0