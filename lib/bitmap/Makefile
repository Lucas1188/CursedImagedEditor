# Makefile for building lib/bitmap/bitmap.c as a standalone test
# Mirrors the structure of lib/zlib/Makefile

CC     = gcc
CFLAGS = -ansi -Werror -Ilib -DSTANDALONE_BITMAP

TARGET = bitmap

SRC = bitmap.c

OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
