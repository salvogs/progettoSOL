#!/bin/bash

# ./bin/client -f socket.sk -p -r '/mnt/c/Users/salvo/Downloads/Istanza-richiesta-diploma-conseguita-maturita.docx','/mnt/c/Users/salvo/Downloads/CLion-2021.1.3.exe' -d mydir -t 8000 &

# ./bin/client -f socket.sk -p -r '/mnt/c/Users/salvo/Downloads/Istanza-richiesta-diploma-conseguita-maturita.docx','/mnt/c/Users/salvo/Downloads/CLion-2021.1.3.exe' -d mydir -t 8000 &
#./bin/client -f socket.sk -p -r '/mnt/c/Users/salvo/Downloads/CLion-2021.1.3.exe' -d mydir -W '/mnt/c/Users/salvo/Downloads/CLion-2021.1.3.exe' -t 5000&
#client -f socket.sk -p -r '/mnt/c/Users/salvo/Downloads/CLion-2021.1.3.exe' -d mydir &


for i in {1..1000}
do
./bin/client -f socket.sk -w src/ &

./bin/client -f socket.sk -R n=0 -u '/mnt/d/CARTELLE/Uni/Secondo Anno/Sistemi Operativi/progettoSOL/src/api.c' -c '/mnt/d/CARTELLE/Uni/Secondo Anno/Sistemi Operativi/progettoSOL/src/server.c' &

./bin/client -f socket.sk -w include/ &
# ./bin/client -f socket.sk -W src/server.c -u '/mnt/d/CARTELLE/Uni/Secondo Anno/Sistemi Operativi/progettoSOL/src/server.c'  -W src/server.c &

# ./bin/client -f socket.sk -c '/mnt/d/CARTELLE/Uni/Secondo Anno/Sistemi Operativi/progettoSOL/src/server.c'

# ./bin/client -f socket.sk -l '/mnt/d/CARTELLE/Uni/Secondo Anno/Sistemi Operativi/progettoSOL/src/server.c' -u '/mnt/d/CARTELLE/Uni/Secondo Anno/Sistemi Operativi/progettoSOL/src/server.c' &

#./bin/client -f socket.sk -p -w include &
done


