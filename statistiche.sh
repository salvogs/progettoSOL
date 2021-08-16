#!/bin/bash


RED='\033[0;31m'
LBLUE='\033[1;34m'
LGREEN='\033[1;32m'
NC='\033[0m' # No Color

if [ $# != 1 ]; then                        # se il numero di parametri di script.sh != 1
    echo usa: $(basename $0) logpath   # stampo su stdout l'usage
    exit 1
fi

logpath=$1
if [ ! -f $logpath ]; then                       # se il parametro inserito non è un file
    echo -e "${RED}L'argomento $logpath non e' un file${NC}"   
    exit 1;   
fi




echo -e "${LGREEN}Numero di operazioni${NC}"

for op in "OPEN FILE" "OPEN-LOCK FILE" "READ FILE" "WRITE FILE" "APPEND TO FILE" "LOCK FILE" "UNLOCK FILE" "CLOSE FILE" "REMOVE FILE"
do
	echo -e "${LBLUE}$op${NC} $(grep -c "op:$op.*" $logpath)"
done
# echo "open-lock $(grep -c "op:OPEN-LOCK FILE.*SUCCESS.*" $logpath)"

# echo "read $(grep -c "op:READ FILE.*SUCCESS.*" $logpath)"

# echo "readn $(grep -c "op:READ N FILE" $logpath)"

# echo "write $(grep -c "op:WRITE FILE.*SUCCESS.*" $logpath)"

# echo "append $(grep -c "op:APPEND TO FILE.*SUCCESS.*" $logpath)"

# echo "lock $(grep -c "op:LOCK FILE.*SUCCESS.*" $logpath)"

# echo "unlock $(grep -c "op:UNLOCK FILE.*SUCCESS.*" $logpath)"

# echo "close $(grep -c "op:CLOSE FILE.*SUCCESS.*" $logpath)"

rsum=$(grep -o "op:READ FILE.*SUCCESS.*" $logpath | cut -d ":" -f 4 | paste -d+ -s | bc) 
rnum=$(grep -c "op:READ FILE.*SUCCESS.*" $logpath)

if [ $rnum -gt 0 ]; then
	echo -e "${LGREEN}Size media letture:${NC} $(echo "scale=2; $rsum/$rnum" | bc) bytes"
fi

wsum=$(grep -o "op:WRITE FILE.*SUCCESS.*\|op:APPEND TO FILE.*SUCCESS.*" $logpath | cut -d ":" -f 4 | paste -d+ -s | bc) 
wnum=$(grep -c "op:WRITE FILE.*SUCCESS.*\|op:APPEND TO FILE.*SUCCESS.*" $logpath)

if [ $wnum -gt 0 ]; then
	echo -e "${LGREEN}Size media scritture:${NC} $(echo "scale=2; $wsum/$wnum" | bc) bytes\n"
fi

echo -e "${LGREEN}Richieste servite da ogni thread${NC}"


# numero di richieste gestite dai thread del server
grep -o "tid.*client:" $logpath |cut -f 1| sort | uniq -c

stats=$(grep -o "==STATS==.*" $logpath)
 
echo -e "\n${LGREEN}Max file memorizzati: ${NC} $(echo $stats | cut -d " " -f 2)"
echo -e "${LGREEN}Max Mbyte memorizzati: ${NC} $(echo $stats | cut -d " " -f 4)"
echo -e "${LGREEN}Max client contemporanei: ${NC} $(echo $stats | cut -d " " -f 6)"

echo -e "${LGREEN}File espulsi: ${NC} $(grep -c "op:ESPULSO" $logpath)"
echo -e "${LGREEN}Algoritmo rimpiazzamento partito ${NC} $(grep -c "op:==ALGORIMO RIMPIAZZAMENTO==" $logpath) ${LGREEN}volte${NC}"







