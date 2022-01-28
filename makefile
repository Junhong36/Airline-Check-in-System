CC=gcc
CFLAGS=-Wall -g -DDEBUG -pthread

all: ACS

ACS: ACS.c
	$(CC) $(CFLAGS) ACS.c -o ACS

.PHONY: clean

clean:
	rm -rf *.exe ACS