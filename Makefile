CC = gcc
CFLAGS = -ansi -Werror -Wall
DFLAGS ?=    # default empty if not provided

SRC := $(shell find src -name "*.c")
LIB := $(shell find lib -name "*.c")

OBJ := $(SRC:%.c=build/%.o) $(LIB:%.c=build/%.o)

TARGET = cursed

.PHONY: all clean force

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DFLAGS) -c $< -o $@

force: clean all

clean:
	rm -rf build $(TARGET)