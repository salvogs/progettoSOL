#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <errno.h>

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


#endif