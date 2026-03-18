all : leeMBR
leeMBR: leeMBR.o leeGPT.o leeAPFS.o
	gcc -g -o leeMBR leeMBR.o leeGPT.o leeAPFS.o -l curses

%.o : %.c
	gcc -g -c $<