#
# CiderPress
# Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
# See the file LICENSE for distribution terms.
#
# DiskImg makefile for Linux.
#
SHELL		= /bin/sh
CC			= gcc
CXX			= g++
#OPT			= -g -D_DEBUG
OPT			= -g -O2
GCC_FLAGS	= -Wall -Wwrite-strings -Wpointer-arith -Wshadow
CXXFLAGS	= $(OPT) $(GCC_FLAGS) -D_FILE_OFFSET_BITS=64

SRCS1		= MDC.cpp
SRCS2		= Convert.cpp
SRCS3		= SSTAsm.cpp
SRCS4		= PackDDD.cpp
SRCS5		= MakeDisk.cpp
SRCS5		= GetFile.cpp

OBJS1		= MDC.o
OBJS2		= Convert.o
OBJS3		= SSTAsm.o
OBJS4		= PackDDD.o
OBJS5		= MakeDisk.o
OBJS6		= GetFile.o

PRODUCT1 = mdc
PRODUCT2 = iconv
PRODUCT3 = sstasm
PRODUCT4 = packddd
PRODUCT5 = makedisk
PRODUCT6 = getfile

DISKIMGLIB	= ../diskimg/libdiskimg.a ../diskimg/libhfs/libhfs.a
NUFXLIB		= ../nufxlib/libnufx.a

all: $(PRODUCT1) $(PRODUCT2) $(PRODUCT3) $(PRODUCT4) $(PRODUCT5) $(PRODUCT6)
	@true

$(PRODUCT1): $(OBJS1) $(DISKIMGLIB)
	$(CXX) -o $@ $(OBJS1) $(DISKIMGLIB) $(NUFXLIB) -lz

$(PRODUCT2): $(OBJS2) $(DISKIMGLIB)
	$(CXX) -o $@ $(OBJS2) $(DISKIMGLIB) $(NUFXLIB) -lz

$(PRODUCT3): $(OBJS3) $(DISKIMGLIB)
	$(CXX) -o $@ $(OBJS3) $(DISKIMGLIB) $(NUFXLIB) -lz

$(PRODUCT4): $(OBJS4) $(DISKIMGLIB)
	$(CXX) -o $@ $(OBJS4) $(DISKIMGLIB) $(NUFXLIB) -lz

$(PRODUCT5): $(OBJS5) $(DISKIMGLIB)
	$(CXX) -o $@ $(OBJS5) $(DISKIMGLIB) $(NUFXLIB) -lz

$(PRODUCT6): $(OBJS6) $(DISKIMGLIB)
	$(CXX) -o $@ $(OBJS6) $(DISKIMGLIB) $(NUFXLIB) -lz

../diskimg/libdiskimg.a:
	(cd ../diskimg ; make)

../diskimg/libhfs/libhfs.a:
	(cd ../diskimg/libhfs ; make)

clean:
	-rm -f *.o core
	-rm -f $(PRODUCT1) $(PRODUCT2) $(PRODUCT3) $(PRODUCT4) $(PRODUCT5)
	-rm -f $(PRODUCT6)
	-rm -f Makefile.bak tags
	-rm -f mdc-log.txt iconv-log.txt makedisk-log.txt

tags::
	@ctags -R --totals *

depend:
	makedepend -- $(CFLAGS) -- $(SRCS1) $(SRCS2) $(SRCS3) $(SRCS4) $(SRCS5) $(SRCS6)

# DO NOT DELETE THIS LINE -- make depend depends on it.
