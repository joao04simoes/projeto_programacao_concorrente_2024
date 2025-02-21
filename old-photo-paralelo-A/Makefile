all: old-photo-serial

old-photo-serial: old-photo-paralelo-A.c image-lib.c image-lib.h
	gcc -pthread old-photo-paralelo-A.c image-lib.c image-lib.h -g -o old-photo-paralelo-A -lgd
