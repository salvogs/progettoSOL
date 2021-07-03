#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
// #include <sys/types.h>
// #include <stdarg.h>
#include "../include/utils.h"


int isNumber(const char* s, long* n){
    if(s==NULL)
        return 1;
    if(strlen(s)==0)
        return 1;

    char* e = NULL;
    errno=0;
    long val = strtol(s, &e, 10);
   
    if(errno == ERANGE) 
        return 2;    // overflow
    if(e != NULL && *e == (char)0){
        *n = val;
        return 0;   // successo 
    }
    return 1;   // non e' un numero
}



ssize_t readn(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount read so far */
        }
        else if (nread == 0)
        {
            puts("eof");
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    puts("fineciclo");
    return (n - nleft); /* return >= 0  (bytes read) */
}


ssize_t writen(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;

    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount written so far */
        }
        else if (nwritten == 0)
            break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n - nleft); /* return >= 0 */
}