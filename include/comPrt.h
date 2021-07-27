#ifndef COM_PROT_H
#define COM_PROT_H

/*
	descrive il protocollo di comunicazione tra client e server
	il formato del messaggio e le corrispondenti MACRO

	RICHIESTA: viene mandata per prima la lunghezza del messaggio che si sta inviando
				tutte le richiesta hanno un campo da un Byte che indica l'operazione
			openFile: 	1Byte(operazione)8Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)1Byte(flags)
			closeFile: 	1Byte(operazione)8Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)
			readFile: 	1Byte(operazione)8Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)
			readNFile:	1Byte(operazione)4Byte(N file da leggere)
			writeFile:	1Byte(operazione)8Byte(dimensione file)dimensione_fileByte(file vero e proprio)
			appendToFile:1Byte(operazione)1Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)8Byte(numero byte da appendere)---byte da appendere
			lockFile:	1Byte(operazione)1Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)
			unlockFile	:1Byte(operazione)1Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)
			removeFile	:1Byte(operazione)1Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)


	RISPOSTA: viene mandato un byte con un intero che indica l'esito dell'operazione
		 ed eventualmente il numero di file espulsi(quindi su cui fare altre read)
*/

typedef struct{
	char* pathname;
	size_t size;

	void* content;	
}ejectedFileT;



#define OP_REQUEST_SIZE 1
#define RESPONSE_SIZE 1
#define MAX_FILESIZE_LEN 10
#define UNIX_PATH_MAX 108 //lunghezza massima indirizzo
// operazioni

#define OPEN_FILE 1
#define CLOSE_FILE 2
#define WRITE_FILE 3
#define READ_FILE 4
#define READ_N_FILE 5
#define REMOVE_FILE 6
// flag openFile

#define O_CREATE 1
#define O_LOCK 2

// risposte server

#define SUCCESS 0
#define SERVER_ERROR 1
#define FILE_EXISTS 2
#define FILE_NOT_EXISTS 3
#define EMPTY_FILE 4
#define BAD_REQUEST 5
#define FILE_LIST 6
#define END_FILE 7



#endif