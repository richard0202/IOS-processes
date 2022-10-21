CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic

.PHONY: all clean zip

all: proj2

proj2: proj2.o
	gcc proj2.o -o proj2 -pthread -lrt

proj2.o: proj2.c functions.c
	gcc $(CFLAGS) -c proj2.c -o proj2.o

clean:
	rm -f *.o proj2

zip:
	zip proj2.zip *.c Makefile
