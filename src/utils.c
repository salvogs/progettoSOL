// #include <stdio.h>
// #include <errno.h>
// #include <string.h>
// #include <stdlib.h>
// #include <unistd.h>
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
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
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


int readFileFromDisk(const char* pathname, void** content, size_t* size){
	
	struct stat info;
	if(stat(pathname, &info) != 0)
		return -1;
	
   

	size_t fsize = info.st_size;

    if(fsize == 0){
        *content = NULL;
        *size = 0;
        return 0;
    }

	FILE* fPtr = fopen(pathname,"r");
	chk_null(fPtr,-1)

	//leggo il file su un buffer

	void* buf = malloc(fsize);
	chk_null(buf,-1)
	
	if(fread(buf,1,fsize,fPtr) != fsize){
		fclose(fPtr);
		free(buf);
		return -1;
	}
	fclose(fPtr);
	
	*content = buf;
	*size = fsize;

	return 0;

}


int writeFileOnDisk(const char* dirname,const char* path, void* buf, size_t size){

    int newLen = strlen(dirname) + strlen(path)+1; // \0  
	char* newPath = calloc(newLen,1);
	ec(newPath,NULL,"calloc",return 1);

	strncpy(newPath,dirname,newLen-1);
    // strncat(newPath,"/",newLen-1);
    strncat(newPath,path,newLen-1);

	char* tmp = strndup(newPath,newLen);
	chk_null(tmp,1);

	for(int i = newLen-1;i >= 0;i--){
		if(tmp[i] == '/'){
			tmp[i] = '\0';
			break;
		}
	}

	char* save = NULL,*token = NULL;
	
	char initialDir[UNIX_PATH_MAX];
	getcwd(initialDir,UNIX_PATH_MAX);

    token = strtok_r(tmp,"/",&save);
	if(tmp[0] == '/'){
        // scrivo a partire dalla root
        if(mkdir(token-1,S_IRWXU) != 0 && errno != EEXIST){
            perror("mkdir");
			free(newPath);
			free(tmp);
            chdir(initialDir);
			return 1;
		}
		chdir(token-1);
		token = strtok_r(NULL,"/",&save);
    }

	while(token){
        errno = 0;
		if(mkdir(token,S_IRWXU) != 0 && errno != EEXIST){
            perror("mkdir");
			free(newPath);
			free(tmp);
            chdir(initialDir);
			return 1;
		}
		chdir(token);
		token = strtok_r(NULL,"/",&save);
	}
	free(tmp);

	chdir(initialDir);

	FILE* fPtr = fopen(newPath,"w+");
	free(newPath);

    if(buf && size){
	    if(fwrite(buf,1,size,fPtr) != size){
            fclose(fPtr);
            return 1;
        }
    }

	fclose(fPtr);
	return 0;
}
