CC = gcc
CFLAGS = -Wall -std=c99 -g -pthread
#TARGETS	= 

SRCDIR = src
HEADDIR = include
BINDIR = bin
OBJDIR = obj
# LIBS = -lpthread

SERVERDEPLIST = $(OBJDIR)/server.o $(OBJDIR)/configParser.o $(OBJDIR)/serverLogger.o
SERVERLIB = lib/libfsApi.a lib/libdataStruct.a lib/libutils.a 

CLIENTDEPLIST = $(OBJDIR)/client.o $(OBJDIR)/clientParser.o 
CLIENTLIB = lib/libclientApi.a lib/libdataStruct.a lib/libutils.a


.PHONY: all clean cleanall

.SUFFIXES: .c

all: $(BINDIR)/server $(BINDIR)/client


test1: test/test1.sh $(BINDIR)/server $(BINDIR)/client
	./test/test1.sh
	./statistiche.sh log.txt

test2: test/test2.sh $(BINDIR)/server $(BINDIR)/client
	./test/test2.sh
	./statistiche.sh log.txt

test3: test/test3.sh $(BINDIR)/server $(BINDIR)/client
	./test/test3.sh
	./statistiche.sh log.txt




$(BINDIR)/server: $(SERVERDEPLIST) $(SERVERLIB) $(HEADDIR)/comPrt.h
	$(CC) $(CFLAGS) $(SERVERDEPLIST) -o $@ $(SERVERLIB)


$(BINDIR)/client: $(CLIENTDEPLIST) $(CLIENTLIB) $(HEADDIR)/comPrt.h
	$(CC) $(CFLAGS) $(CLIENTDEPLIST) -o $@ $(CLIENTLIB)

#$@ : nome del target 
#$^ : la dependency list
#$< : il primo nome nella dependency list

lib/libclientApi.a: $(OBJDIR)/api.o
	ar rvs $@ $^


lib/libfsApi.a: $(OBJDIR)/fs.o
	ar rvs $@ $^


lib/libdataStruct.a: $(OBJDIR)/icl_hash.o $(OBJDIR)/queue.o
	ar rvs $@ $^

lib/libutils.a: $(OBJDIR)/utils.o
	ar rvs $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADDIR)/%.h
	$(CC) -c $(CFLAGS) $< -o $@


cleanall: clean
	rm -f -r test/readDir test/ejectedDir lib/*.a obj/*.o 

clean:
	rm -f bin/*
