BIN = leeMBR

all: $(BIN)

SOURCE = \
		leeMBR.c \
		leeGPT.c \
		leeAPFS.c \
		checksum.c \

CC = gcc
CC_FLAGS = -g -Wall
LIB_FLAGS = -l curses

OBJ = $(SOURCE:%.c=%.o)

$(BIN) : $(OBJ)
		echo Ligando...
		#Solo liga
		$(CC) $(OBJ) $(LIB_FLAGS) -o $(BIN)

%.o : %.c
		#Compila mismo nombre en .o
		$(CC) $(CC_FLAGS) -c $<

.PHONY: clean
clean:
		echo Limpiando...
		~rm -r $(OBJ)
		~rm -r $(BIN)