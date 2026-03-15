CC = gcc
CFLAGS = -ansi -Werror

SRC := $(shell find src -name "*.c")
LIB := $(shell find lib -name "*.c")

OBJ := $(SRC:%.c=build/%.o) $(LIB:%.c=build/%.o)

TARGET = cursed

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)