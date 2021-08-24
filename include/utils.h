#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>

char *strndup(const char *s, size_t n);


#define UNIX_PATH_MAX 108 //lunghezza massima indirizzo


// Macro controllo errori
#define ec(s, r, m, e) do{\
    if ((s) == (r))    \
    {   \
        if(errno)               \
            perror(m);     \
        e;           \
    }\
}while(0);

#define ec_n(s, r, m, e) do{\
    if ((s) != (r))    \
    {   \
        if(errno)               \
            perror(m);     \
        e;           \
    }\
}while(0);

#define chk_null(s,r) \
    if((s) == NULL) \
    {               \
        return r;   \
    }       

#define chk_null_op(s,op,r) \
    if((s) == NULL) \
    {               \
        (op);         \
        return r;   \
    }       

#define chk_1(s,r) \
    if((s) == 1) \
    {               \
        return r;   \
    }       


#define chk_neg1(s,r) \
    if((s) == -1) \
    {               \
        return r;   \
    }       

#define chk_neg1_op(s,op,r) \
    if((s) == -1) \
    {               \
        (op);         \
        return r;   \
    }       

// die on SERVER_ERROR
#define die_on_se(s) \
    if((s) == SERVER_ERROR) \
    {               \
        exit(EXIT_FAILURE);   \
    }

// chk read e write verso i client
#define chk_get_send(s) if(1){\
    int r = s;\
    if(r == SERVER_ERROR && errno != EPIPE && errno != ECONNRESET && errno != EBADF) \
    {               \
        exit(EXIT_FAILURE);   \
    }\
    if(r == -1) \
    {               \
        continue;   \
    }       \
}



#define CREA_THREAD(tid,param,fun,args) if(pthread_create(tid,param,fun,args) != 0){ \
	fprintf(stderr,"errore creazione thread\n"); \
	exit(EXIT_FAILURE);}



#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { printf("ERRORE FATALE lock\n"); exit(EXIT_FAILURE);}
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { printf("ERRORE FATALE unlock\n"); exit(EXIT_FAILURE);}
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { printf("ERRORE FATALE wait\n"); exit(EXIT_FAILURE);}
#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       { printf("ERRORE FATALE signal\n"); exit(EXIT_FAILURE);}




int isNumber(const char* s, long* n);


ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);

int readFileFromDisk(const char* pathname, void** content, size_t* size);

int writeFileOnDisk(const char* dirname,const char* path, void* buf, size_t size);

#endif