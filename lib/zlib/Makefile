# Makefile for building lib/zlib/zlib.c with all dependencies in lib/

CC = gcc
CFLAGS = -ansi -Werror -Ilib

# Target executable
TARGET = zlib

# Automatically find all .c files under lib/
SRC = $(shell find ../ -name "*.c")

# Corresponding object files
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)