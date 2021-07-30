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


char *strndup(const char *s, size_t n);


#define UNIX_PATH_MAX 108 //lunghezza massima indirizzo


// Macro controllo errori
#define ec(s, r, m, e) \
    if ((s) == (r))    \
    {   \
        if(errno)               \
            perror(m);     \
        e;           \
    }

#define ec_n(s, r, m, e) \
    if ((s) != (r))    \
    {   \
        if(errno)               \
            perror(m);     \
        e;           \
    }

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

    
// ritorna
//   0: ok
//   1: non e' un numero
//   2: overflow/underflow
//
int isNumber(const char* s, long* n);




/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
ssize_t readn(int fd, void *ptr, size_t n);


/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
ssize_t writen(int fd, void *ptr, size_t n);




int readFileFromDisk(const char* pathname, void** content, size_t* size);

int writeFileOnDisk(const char* dirname,const char* path, void* buf, size_t size);

#endif