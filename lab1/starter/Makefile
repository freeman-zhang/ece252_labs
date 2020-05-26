CC = gcc       # compiler
LD = gcc       # linker
CFLAGS = -Wall -g -std=c99 # compilation flags
LDLIBS = -lz   # link with libz

LIB_UTIL = ./png_util/zutil.c ./png_util/crc.c 

TARGETS = catpng pnginfo findpng

default: all

all: ${TARGETS}

catpng: catpng.c $(LIB_UTIL)
	$(LD) -o $@ $^ $(LDLIBS) 

pnginfo: pnginfo.c 
	$(LD) -o $@ $^ 

findpng: findpng.c 
	$(LD) -o $@ $^


.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
