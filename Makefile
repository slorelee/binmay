all: binmay

binmay: binmay.c
	gcc -Wall -o binmay binmay.c

binmay-mingw: binmay.c
	/usr/bin/i586-mingw32msvc-gcc -Wall -o binmay-mingw binmay.c


clean:
	rm -f binmay
