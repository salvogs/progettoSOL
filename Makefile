CC = gcc
CFLAGS = -Wall -std=c99 -pthread -g
#TARGETS	= 

SRCDIR = src
HEADDIR = include
BINDIR = bin
LIBS = -lpthread

SERVERDEPLIST = $(SRCDIR)/server.c $(SRCDIR)/configParser.c $(SRCDIR)/queue.c $(SRCDIR)/utils.c

CLIENTDEPLIST = $(SRCDIR)/client.c $(SRCDIR)/clientParser.c $(SRCDIR)/api.c $(SRCDIR)/queue.c $(SRCDIR)/utils.c


.PHONY: all clean

.SUFFIXES: .c

all: $(BINDIR)/server $(BINDIR)/client


$(BINDIR)/server: $(SERVERDEPLIST)
	$(CC) $(CFLAGS) $^ -o $@


$(BINDIR)/client: $(CLIENTDEPLIST)
	$(CC) $(CFLAGS) $^ -o $@


	
clean:
	-rm -f bin/*
