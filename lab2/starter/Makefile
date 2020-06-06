CC = gcc
LD = gcc
CFLAGS = -Wall -g -std=c99
LDLIBS = -lz -lcurl

LIB_UTIL = ./png_util/zutil.c ./png_util/crc.c ./png_util/lab_png.c ./cURL/curl_util.c

TARGETS = paster

default: all

all: ${TARGETS}

paster: paster.c $(LIB_UTIL)
	$(LD) -o $@ $^ $(LDLIBS) 

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
