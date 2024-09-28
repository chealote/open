OUTNAME=open
SRC=main.c
CFLAGS=-Wall -Wextra

all: build

build:
	$(CC) $(CFLAGS) -o $(OUTNAME) $(SRC)
