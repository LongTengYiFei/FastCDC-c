CC = gcc
FLAG = -g
LIB = -lcrypto -lz
target:
	$(CC) $(FLAG) fastcdc.c $(LIB) -o fastcdc64
clean:
	rm fastcdc64
