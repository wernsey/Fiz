CC=gcc

ifeq ($(BUILD),debug)
# Debug mode flags
CFLAGS = -Wall -Werror -pedantic -O0 -g
LFLAGS = -lm
else
# Release mode
CFLAGS = -Wall -Werror -pedantic -O2 -DNDEBUG
LFLAGS = -s -fno-exceptions -lm
endif

all: fiz

debug:
	make "BUILD=debug"

fiz: shell.o libfiz.a
	gcc -o $@ $^ $(LFLAGS)

shell.o: shell.c fiz.h
	gcc -o $@ $(CFLAGS) -c $<
	
libfiz.a: fiz.o hash.o expr.o auxfuns.o
	ar rs $@ $^

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@
	
fiz.o: fiz.h hash.h

auxfuns.o: fiz.h

hash.o: hash.c hash.h

expr.o: 

clean:
	-rm -rf fiz fiz.exe
	-rm -rf libfiz.a
	-rm -rf *.o
	-rm -rf *~
