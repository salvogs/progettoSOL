#ifndef API_H
#define API_H


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>     
#include <sys/socket.h>
#include <sys/un.h>/* struttura che rappresenta un indirizzo */
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include "../include/comPrt.h"
//#include "../include/client.h"
#define UNIX_PATH_MAX 108


// char* intToOp(int op){
// 	switch(op){
// 		case OPEN_FILE: return "OPEN FILE"; break;
// 		case CLOSE_FILE: return "CLOSE FILE"; break;
// 		case WRITE_FILE: return "WRITE FILE"; break;
// 	}
// }

// a = arg r = retvalue  
// nel caso di RECIVED_FILE op funge da bytes del file
	//	fprintf(stdout,"PID: %d ", getpid());
#define PRINTER(op,a,r)  \
	if(PRINTS){\
		switch(r){\
			case SUCCESS:\
				fprintf(stdout,"%s : %s : SUCCESSO\n",op,a);\
			break;\
			case EMPTY_FILE:\
				fprintf(stdout,"%s : %s : SUCCESSO(FILE VUOTO)\n",op,a);\
			break;\
			case FILE_EXISTS:\
				fprintf(stdout,"%s : %s : FILE GIA' ESISTENTE\n",op,a);\
			break;\
			case FILE_NOT_EXISTS:\
				fprintf(stdout,"%s : %s : FILE NON TROVATO\n",op,a);\
			break;\
			case SERVER_ERROR:\
				fprintf(stdout,"%s : %s : ERRORE INTERNO SERVER\n",op,a);\
			break;\
			case BAD_REQUEST:\
				fprintf(stdout,"%s : %s : RICHIESTA INCOMPLETA\n",op,a);\
			break;\
			case FILE_TOO_LARGE:\
				fprintf(stdout,"%s : %s : FILE TROPPO GRANDE\n",op,a);\
			break;\
			case NOT_LOCKED:\
				fprintf(stdout,"%s : %s : NESSUNA LOCK SUL FILE\n",op,a);\
			break;\
			default:;break;\
		}\
	}\

#define PRINT(a)\
	if(PRINTS)	(a);




int FD_CLIENT;
int PRINTS; //se 1 le stampe per ogni operazione sono abilitate



/*
Viene aperta una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la
richiesta di connessione, la connessione da parte del client viene ripetuta dopo ‘msec’ millisecondi e fino allo
scadere del tempo assoluto ‘abstime’ specificato come terzo argomento.

ritorna
0:	successo
-1:	fallimento (setta errno)
*/
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/*
Chiude la connessione AF_UNIX associata al socket file sockname. 

ritorna
0:	successo
-1:	fallimento (setta errno)
*/
int closeConnection(const char* sockname);


/*
Richiesta di apertura o di creazione di un file. La semantica della openFile dipende dai flags passati come secondo
argomento che possono essere O_CREATE ed O_LOCK. Se viene passato il flag O_CREATE ed il file esiste già
memorizzato nel server, oppure il file non esiste ed il flag O_CREATE non è stato specificato, viene ritornato un
errore. In caso di successo, il file viene sempre aperto in lettura e scrittura, ed in particolare le scritture possono
avvenire solo in append. Se viene passato il flag O_LOCK (eventualmente in OR con O_CREATE) il file viene
aperto e/o creato in modalità locked, che vuol dire che l’unico che può leggere o scrivere il file ‘pathname’ è il
processo che lo ha aperto. Il flag O_LOCK può essere esplicitamente resettato utilizzando la chiamata unlockFile

ritorna
0:	successo
-1:	fallimento (setta errno)
*/
int openFile(const char* pathname, int flags);


/*
Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore ad un'area allocata sullo heap nel
parametro ‘buf’, mentre ‘size’ conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In
caso di errore, ‘buf‘e ‘size’ non sono validi. 

ritorna
0:	successo
-1:	fallimento (setta errno)
*/
int readFile(const char* pathname, void** buf, size_t* size);

/*
Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client. Se il server
ha meno di ‘N’ file disponibili, li invia tutti. Se N<=0 la richiesta al server è quella di leggere tutti i file
memorizzati al suo interno. 
ritorna
n >=0:	successo (ritorna il n. di file effettivamente letti)
-1:		fallimento (setta errno)
*/
int readNFiles(int N, const char* dirname);

/*
Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
terminata con successo, è stata openFile(pathname, O_CREATE| O_LOCK). Se ‘dirname’ è diverso da NULL, il
file eventualmente spedito dal server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere
scritto in ‘dirname’.

ritorna
0:	successo
-1:	fallimento (setta errno)

*/
int writeFile(const char* pathname, const char* dirname);

/*
Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’.


ritorna
0:	successo
-1:	fallimento (setta errno)

*/
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/*
In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la
richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non
viene resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è specificato. 


ritorna
0:	successo
-1:	fallimento (setta errno)
*/
int lockFile(const char* pathname);


/*
Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se l’owner della lock è il processo che
ha richiesto l’operazione, altrimenti l’operazione termina con errore.


ritorna
0:	successo
-1:	fallimento (setta errno)
*/
int unlockFile(const char* pathname);

/*
Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.

ritorna
0:	successo
-1:	fallimento (setta errno)
*/
int closeFile(const char* pathname);


/*
Rimuove il file cancellandolo dal file storage server. L’operazione fallisce se il file non è in stato locked, o è in
stato locked da parte di un processo client diverso da chi effettua la removeFile.
*/
int removeFile(const char* pathname);


int getFile(size_t* size,void** content, char** pathname);
int getResponseCode(int fd);
int sendRequest(int fd, void* req, int len);
int getPathname(char** pathname);
int getEjectedFile(int response, const char* dirname, size_t* bytes, int* readCounter);
#endif