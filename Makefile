CC=gcc
CFLAGS=-std=c99 -Wall -O2 -Wextra -fdiagnostics-color=always
LDFLAGS=-lm

all: grtz

grtz: main.o
	gcc main.o -o qkdec $(LDFLAGS)

main.o: main.c

clean:
	-rm -f qkdec *.o

plot:
	gnuplot-qt -p plot.gp

.PHONY: clean plot
