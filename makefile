#CS 360 Lab A Makefile

CC = gcc 

INCLUDES = -I/home/plank/cs360/include

CFLAGS = -g -lpthread $(INCLUDES)

LIBDIR = /home/plank/cs360/objs

LIBS = $(LIBDIR)/libfdr.a

EXECUTABLES = chat-server

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

chat-server: chat-server.o
	$(CC) $(CFLAGS) -o chat-server chat-server.o sockettome.c $(LIBS)

#make clean will rid your directory of the executable,
#object files, and any core dumps you've caused
clean:
	rm $(EXECUTABLES) *.o tmp*

