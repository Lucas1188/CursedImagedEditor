CC = gcc
CFLAGS = -ansi -Werror -Wall
DFLAGS ?=

# =========================
# SOURCE BUILD
# =========================
SRC := $(shell find src -type f -name "*.c")
LIB := $(shell find lib -type f -name "*.c")

OBJ := $(SRC:%.c=build/%.o) $(LIB:%.c=build/%.o)

TARGET = cursed

.PHONY: all clean payload html url bundle

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET) dist

# =========================
# OUTPUT PIPELINE
# =========================
DIST = dist
BIN_B64 = $(DIST)/payload.b64
HTML = $(DIST)/index.html
URL = $(DIST)/url.txt

payload: $(TARGET)
	mkdir -p $(DIST)
	base64 $(TARGET) | tr -d '\n' > $(BIN_B64)

html: payload
	mkdir -p $(DIST)
	sed 's|__PAYLOAD__|PLACEHOLDER|' index.template.html > $(HTML).tmp
	awk 'BEGIN{while((getline line < "$(BIN_B64)")>0) b64=b64 line} {gsub("PLACEHOLDER", b64); print}' $(HTML).tmp > $(HTML)
	rm $(HTML).tmp

url: html
	@echo "data:text/html;base64,$$(base64 < $(HTML) | tr -d '\n')" > $(URL)
	@echo "[OK] generated $(URL)"

bundle: url