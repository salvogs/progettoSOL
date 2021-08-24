#ifndef CLIENT_H
#define CLIENT_H

int isRegularFile(char* pathname);
int isDirectory(char* pathname);
int isPointDir(char *dir);
int recursiveVisit(char* pathname,long* n,int limited,char* dirname);
int writeHandler(char opt,char* args,char* dirname,struct timespec* reqTime);
int readHandler(char opt,char* args,char* dirname,struct timespec* reqTime);
int lockUnlockHandler(char opt,char* args,struct timespec* reqTime);
int removeHandler(char* args,struct timespec* reqTime);


#endif