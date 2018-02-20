CC=gcc
CFLAGS=-std=c99 -Wall -O2 -Wextra -fdiagnostics-color=always
LDFLAGS=-lm

all: grtz

grtz: main.o

main.o: main.c

clean:
	-rm -f grtz *.o

plot:
	gnuplot-qt -p plot.gp

.PHONY: clean plot
