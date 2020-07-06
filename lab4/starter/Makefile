LD = gcc
LDFLAGS = -Wall -g -std=gnu99 -ggdb3
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS = $(LDLIBS_XML2) $(LDLIBS_CURL) -lz -pthread


LIB_UTIL = ./png_util/zutil.c ./png_util/crc.c ./png_util/lab_png.c ./cURL/curl_util.c 

TARGETS = findpng2

default: all

all: ${TARGETS}

findpng2: findpng2.c $(LIB_UTIL)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) log.txt