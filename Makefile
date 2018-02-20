CC=gcc
CFLAGS=-std=c99 -Wall -O2 -Wextra -fdiagnostics-color=always
LDFLAGS=-lm

all: qkdec

qkdec: main.o
	gcc main.o -o qkdec $(LDFLAGS)

main.o: main.c

clean:
	-rm -f qkdec *.o page_test.raw page_test.out

plot: run
	gnuplot-qt -p -c plot.gp page_test.out

page_test.raw: page_test.ogg
	sox page_test.ogg -b 16 -r 8000 page_test.raw

run: page_test.raw
	./qkdec page_test.raw >page_test.out

test: run plot
	

.PHONY: clean plot test run
