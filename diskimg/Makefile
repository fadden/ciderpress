#
# DiskImg makefile for Linux.
#
SHELL		= /bin/sh
CC			= gcc
CXX			= g++
AR			= ar
OPT			= -g -D_DEBUG
#-DEXCISE_GPL_CODE
#OPT			= -g -O2
GCC_FLAGS	= -Wall -Wwrite-strings -Wpointer-arith -Wshadow
# -Wstrict-prototypes
CXXFLAGS	= $(OPT) $(GCC_FLAGS) -D_FILE_OFFSET_BITS=64

SRCS		= ASPI.cpp CFFA.cpp Container.cpp CPM.cpp DDD.cpp DiskFS.cpp \
			  DiskImg.cpp DIUtil.cpp DOS33.cpp DOSImage.cpp FAT.cpp FDI.cpp \
			  FocusDrive.cpp \GenericFD.cpp Global.cpp Gutenberg.cpp HFS.cpp \
			  ImageWrapper.cpp MacPart.cpp MicroDrive.cpp Nibble.cpp \
			  Nibble35.cpp OuterWrapper.cpp OzDOS.cpp Pascal.cpp ProDOS.cpp \
			  RDOS.cpp TwoImg.cpp UNIDOS.cpp VolumeUsage.cpp Win32BlockIO.cpp
OBJS		= ASPI.o CFFA.o Container.o CPM.o DDD.o DiskFS.o \
			  DiskImg.o DIUtil.o DOS33.o DOSImage.o FDI.o \
			  FocusDrive.o FAT.o GenericFD.o Global.o Gutenberg.o HFS.o \
			  ImageWrapper.o MacPart.o MicroDrive.o Nibble.o \
			  Nibble35.o OuterWrapper.o OzDOS.o Pascal.o ProDOS.o \
			  RDOS.o TwoImg.o UNIDOS.o VolumeUsage.o Win32BlockIO.o

STATIC_PRODUCT	= libdiskimg.a
PRODUCT = $(STATIC_PRODUCT)

all: $(PRODUCT)
	@true

$(STATIC_PRODUCT): $(OBJS)
	-rm -f $(STATIC_PRODUCT)
	$(AR) rcv $@ $(OBJS)

clean:
	-rm -f *.o core
	-rm -f $(STATIC_PRODUCT)
	-rm -f Makefile.bak

tags::
	@ctags -R --totals *

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

# DO NOT DELETE THIS LINE -- make depend depends on it.
