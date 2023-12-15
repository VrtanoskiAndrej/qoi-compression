CC = gcc
CFLAGS = -O3 -Wall
DEBUG_FLAGS = -D DEBUG -g -Wall
LIBS = -lpng

all: qoiconvert

qoiconvert: main.o qoi.o
	$(CC) -o qoiconvert main.c qoi.c $(LIBS) $(CFLAGS)

.PHONY: debug
debug: 
	$(CC) -o qoiconvert main.c qoi.c $(LIBS) $(DEBUG_FLAGS)

.PHONY: asm
asm: 
	$(CC) -S main.c qoi.c $(LIBS) $(CFLAGS)

.PHONY: clean
clean:
	rm -f *.o *.S qoiconvert
