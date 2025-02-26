main: main.c tpool.o
	gcc -O2 -g -Wall main.c tpool.o -o main

tpool.o: tpool.c
	gcc -c tpool.c

clean:
	rm *.o main