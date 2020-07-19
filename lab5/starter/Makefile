LD = gcc
LDFLAGS = -Wall -g -std=gnu99 -ggdb3
CFLAGS_XML2 = $(shell xml2-config --cflags)
CFLAGS_CURL = $(shell curl-config --cflags)
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS = $(LDLIBS_XML2) $(LDLIBS_CURL) $(CFLAGS_XML2) $(CFLAGS_CURL) $(shell pkg-config --libs libxml-2.0 libcurl) -lz -pthread 

LIB_UTIL =./cURL/curl_util.c 

TARGETS= findpng3
default: all

all: ${TARGETS}

findpng3: findpng3.c $(LIB_UTIL)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 


.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS)
