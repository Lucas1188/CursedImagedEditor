# Toolchain Selector: 'zig' (default) or 'gcc'
MODE ?= zig
OPT ?= -O3
# Original Flags & Includes
CFLAGS   = -ansi -Wall -Werror $(OPT)
INCLUDES = -Ilib -Isrc
DIST      = dist
ALL_C     = $(shell find lib src -name "*.c")

# Artifact Paths
BIN_NAMES = cursed-linux cursed.exe cursed-mac-x86 cursed-mac-arm

# 2. Use the toggle ONLY to switch compilers
ifeq ($(MODE), zig)
	CC_LINUX  = zig cc -target x86_64-linux-gnu
    CC_WIN    = zig cc -target x86_64-windows-gnu
    CC_PACKER = zig cc
else
    # Classic Mode (GCC / MinGW)
    CC_LINUX  = gcc
    CC_WIN    = x86_64-w64-mingw32-gcc
    CC_PACKER = gcc
endif

BINS      = $(addprefix $(DIST)/, $(BIN_NAMES))
PACKER    = $(DIST)/packer
TPL       = index.template.html
TDL       = delivery.template.html
UNPACKER  = $(DIST)/dist.html
URL_FILE  = $(DIST)/url.txt
FINAL_OUT = $(DIST)/cursed-delivery.html
FINAL_PDF = $(DIST)/Submission_Manifest_GroupX.pdf

.PHONY: all bundle deliver pdf clean prepare

all: clean deliver pdf measure-artifacts

measure-artifacts: $(BINS)
	@echo ">>> Measuring Resident Artifacts..."
	@for bin in $(BINS); do \
		if [ -f $$bin ]; then \
			size=$$(stat -c%s "$$bin"); \
			echo "  [$$bin]: $${size} bytes"; \
		else \
			echo "  [$$bin]: [MISSING]"; \
		fi; \
	done

prepare:
	@mkdir -p $(DIST)

# The Packer is always built for the HOST machine
$(PACKER): $(ALL_C) | prepare
	@echo ">>> Building Host Packer ($(CC_PACKER))..."
	@$(CC_PACKER) $(CFLAGS) -DBUILD_PACKER $(ALL_C) $(INCLUDES) -o $@

# Linux Target
$(DIST)/cursed-linux: $(ALL_C) | prepare
	@echo ">>> Compiling Linux via $(CC_LINUX)..."
	@$(CC_LINUX) $(CFLAGS) -DBUILD_ENGINE $(ALL_C) $(INCLUDES) -o $@

# Windows Target
$(DIST)/cursed.exe: $(ALL_C) | prepare
	@echo ">>> Compiling Windows via $(CC_WIN)..."
	@$(CC_WIN) $(CFLAGS) -DBUILD_ENGINE $(ALL_C) $(INCLUDES) -o $@

# Mac Targets (Only active in Zig mode)
$(DIST)/cursed-mac-%: $(ALL_C) | prepare
ifeq ($(MODE), zig)
	@echo ">>> Cross-compiling Mac $*..."
	@zig cc -target $(if $(findstring x86,$*),x86_64-macos,aarch64-macos) $(CFLAGS) -DBUILD_ENGINE $(ALL_C) $(INCLUDES) -o $@
else
	@echo ">>> [STUB] Creating byte-bearing placeholder for $*..."
	@echo "This binary was built in MODE=gcc. Re-run with MODE=zig to generate the native Mac artifact." > $@
endif

bundle: $(PACKER) $(BINS)
	@echo ">>> Checking Resident Dependencies..."
	@if [ ! -f $(DIST)/pako.min.js ]; then \
		(cp /opt/pako.min.js $(DIST)/pako.min.js 2>/dev/null || \
		curl -s https://cdnjs.cloudflare.com/ajax/libs/pako/2.1.0/pako.min.js > $(DIST)/pako.min.js); \
	fi
	@echo ">>> Stitching master template..."
	@sed -e '/__PAKO_MIN_JS__/{r $(DIST)/pako.min.js' -e 'd}' $(TPL) > $(UNPACKER)
	@echo ">>> Generating Master Data URL..."
#	cd into dist to so relative paths to binaries work correctly in the packer command
	@(cd $(DIST) && ./packer dist.html $(BIN_NAMES) > url.txt)

deliver: bundle
	@echo ">>> Generating HTML Delivery..."
	@./$(PACKER) -delivery $(TDL) $(URL_FILE) > $(FINAL_OUT)

pdf: bundle
	@echo ">>> Generating PDF Delivery..."
	@./$(PACKER) -pdf $(FINAL_PDF) $(URL_FILE)

clean:
	@rm -rf $(DIST)