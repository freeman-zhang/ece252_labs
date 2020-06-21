CC = gcc
LD = gcc
CFLAGS = -Wall -g -std=c99 -ggdb3
LDLIBS = -lz -lcurl -pthread

LIB_UTIL = ./png_util/zutil.c ./png_util/crc.c ./png_util/lab_png.c ./cURL/curl_util.c

TARGETS = paster2

default: all

all: ${TARGETS}

run:
	./paster2 5 5 4 200 1 

paster2: paster2.c $(LIB_UTIL)
	$(LD) $(CFLAGS) -o $@ $^ $(LDLIBS) 

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
