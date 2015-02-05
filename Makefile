# Makefile

all: mond example

mond: mond.c mond.h
	gcc -Wall -Werror -g -o $@ mond.c mond.h

example: example.c
	gcc -Wall -Werror -g -o $@ example.c

clean:
	rm -f *.o mond example

