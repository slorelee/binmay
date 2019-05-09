all: binmay

binmay: binmay.c
	gcc -Wall -o binmay binmay.c

clean:
	rm -f binmay
