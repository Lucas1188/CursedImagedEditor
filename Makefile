CC_LINUX = gcc
CC_WIN   = x86_64-w64-mingw32-gcc
CFLAGS   = -ansi
INCLUDES = -Ilib -Isrc

ALL_C    = $(shell find lib src -name "*.c")
BINS     = cursed-linux cursed.exe
PACKER   = ./packer
TPL      = index.template.html
UNPACKER = dist.html
URL_FILE = url.txt
TDL	  	 = delivery.template.html
FINAL_OUT = cursed-delivery.html
FINAL_PDF    = Submission_Manifest_GroupX.pdf

.PHONY: all bundle clean

all: deliver

$(PACKER): $(ALL_C)
	$(CC_LINUX) $(CFLAGS) -DBUILD_PACKER $(ALL_C) $(INCLUDES) -o $@

cursed-linux: $(ALL_C)
	$(CC_LINUX) $(CFLAGS) -DBUILD_ENGINE $(ALL_C) $(INCLUDES) -o $@

cursed.exe: $(ALL_C)
	$(CC_WIN) $(CFLAGS) -DBUILD_ENGINE $(ALL_C) $(INCLUDES) -o $@

bundle: $(PACKER) $(BINS)
	@echo "Fetching Pako for serverless autonomy..."
	@curl -s https://cdnjs.cloudflare.com/ajax/libs/pako/2.1.0/pako.min.js > pako.min.js
#We use Pako to handle decompression of our url as a form of validation that our program is correctly encoding and packing the data. And we were not about to write our own decompressor in JS just for that, so here we are.
	@echo "Stitching master template..."
#This is the only time we use sed in the entire process, so we can be a bit hacky and just insert the pako.min.js content directly into the HTML template. This keeps our bundle self-contained without needing to manage multiple files.
	@sed -e '/__PAKO_MIN_JS__/{r pako.min.js' -e 'd}' index.template.html > $(UNPACKER)	
	@echo ">>> Generating Master Data URL via Our-Packer..."
	./$(PACKER) $(UNPACKER) $(BINS) > url.txt
	@echo "[SUCCESS] Data URL written to url.txt"

deliver: bundle
	@echo ">>> Assembling Final Delivery Artifact..."
	@$(PACKER) -delivery $(TDL) $(URL_FILE) > $(FINAL_OUT)
	@echo "[SUCCESS] The file is now LITERALLY in $(FINAL_OUT)"

pdf: bundle
	@echo ">>> Encapsulating buffer into Binary PDF Carrier..."
	@$(PACKER) -pdf $(FINAL_PDF) $(URL_FILE)
	@echo "[SUCCESS] PDF Manifest created at $(FINAL_PDF)"

clean:
	rm -rf $(BINS) $(PACKER) url.txt