#include <stdio.h>
#include <string.h> //
#include "../include/utils.h"
#include "../include/api.h"

int openConnection(const char* sockname, int msec, const struct timespec abstime){
	//file descriptor client FD_CLIENT
	
	//creazione client socket
	ec(FD_CLIENT=socket(AF_UNIX,SOCK_STREAM,0),-1,"client socket",return -1);

	struct sockaddr_un sa;
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX; //famiglia indirizzo

	
	
	printf("connessione..\n");

	time_t t; //ritorna il tempo trascorso da 00:00:00 UTC, January 1, 1970
	
	struct timespec tsleep = {msec/1000,(msec % 1000)*1000000}; //modulo per overflow campo .tv_nsec
	
	while(connect(FD_CLIENT,(struct sockaddr *)&sa,sizeof(sa)) == -1){
		t = time(NULL); //ritorna il tempo trascorso da 00:00:00 UTC, January 1, 1970

		if(errno == ENOENT){ //socket non esiste ancora
			if(t >= abstime.tv_nsec){
				return -1;
			}
			if(PRINTS == 1)
				fprintf(stdout,"Impossibile stabilire una connessione con il server. Riprovo tra %d msec\n",msec);
			
		}else{
			return -1;
		}
		//sleep(1);
		nanosleep(&tsleep,NULL);

	}


	printf("connessione stabilita\n");
	return 0;
	

	
	return 0;
	
	
}