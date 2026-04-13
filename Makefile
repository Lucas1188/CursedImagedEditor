CC_LINUX = gcc
CC_WIN   = x86_64-w64-mingw32-gcc
CFLAGS   = -ansi
INCLUDES = -Ilib -Isrc

ALL_C    = $(shell find lib src -name "*.c")
BINS     = cursed-linux cursed.exe
PACKER   = ./packer
TPL      = index.template.html

.PHONY: all bundle clean

all: bundle

$(PACKER): $(ALL_C)
	$(CC_LINUX) $(CFLAGS) -DBUILD_PACKER $(ALL_C) $(INCLUDES) -o $@

cursed-linux: $(ALL_C)
	$(CC_LINUX) $(CFLAGS) -DBUILD_ENGINE $(ALL_C) $(INCLUDES) -o $@

cursed.exe: $(ALL_C)
	$(CC_WIN) $(CFLAGS) -DBUILD_ENGINE $(ALL_C) $(INCLUDES) -o $@

# The final "bundle" is now just the output of the packer redirected to a file
bundle: $(PACKER) $(BINS)
	@echo ">>> Generating Master Data URL via C-Packer..."
	./$(PACKER) $(TPL) $(BINS) > url.txt
	@echo "[SUCCESS] Data URL written to url.txt"

clean:
	rm -rf $(BINS) $(PACKER) url.txt