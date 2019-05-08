#
# file:    Makefile
#
LDLIBS = -ldmalloc
CFLAGS = -g

all: test

# default build rules:
# .c to .o: $(CC) $(CFLAGS) file.c -c -o file.o
# multiple .o to exe. : $(CC) $(LDFLAGS) file.o [file.o..] -o exe

test: test.o qthread.o stack.o switch.o

server: server.o qthread.o stack.o switch.o
