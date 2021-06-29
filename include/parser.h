#ifndef PARSER_H
#define PARSER_H

#include "../include/queue.h"

// Macro controllo errori
#define ec(s, r, m, e) \
    if ((s) == (r))    \
    {                  \
        perror(m);     \
        e;           \
    }

#define UNIX_PATH_MAX 108 //lunghezza massima indirizzo

#define P_w \
	if(p_flag == 1) \
		fprintf("scrivooo");

typedef struct opType{
	char opt;
	char* arg;
}opType;




#define HELP_MESSAGE \
	"-h | -f filename | -w dirname[,n=0] | -W file1[,file2]\n"\
	"| -D dirname | -r file1[,file2] | -R [n=0] | -d dirname \n| -t time | -l file1[,file2]\n"\
	"|-u file1[,file2] | -c file1[,file2] | -p\n"
	
#define UNIX_PATH_MAX 108 //lunghezza massima indirizzo



queue* parser(int argc, char* argv[]);
int enqueueArg(queue* argQueue, char opt,char* arg);



#endif