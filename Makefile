CC=gcc
CFLAGS=-std=c99 -Wall -O2 -Wextra -fdiagnostics-color=always $(shell pkg-config --cflags sndfile)
LDFLAGS=-lm $(shell pkg-config --libs sndfile)

all: qkdec

qkdec: main.o
	gcc main.o -o qkdec $(LDFLAGS)

main.o: main.c

clean:
	-rm -f qkdec *.o

plot: run
	gnuplot-qt -p -c plot.gp page_test.out

run: qkdec
	-./qkdec page_test.ogg >page_test.out

test: run plot
	

.PHONY: clean plot test run
