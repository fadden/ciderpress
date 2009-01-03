#
# DiskImg libhfs makefile for Linux.
#
SHELL		= /bin/sh
CC			= gcc
AR			= ar
OPT			= -g -DHAVE_CONFIG_H
#OPT			= -g -O2 -DHAVE_CONFIG_H
GCC_FLAGS	= -Wall -Wwrite-strings -Wpointer-arith -Wshadow -Wstrict-prototypes
CFLAGS	= $(OPT) $(GCC_FLAGS) -D_FILE_OFFSET_BITS=64

SRCS		= os.c data.c block.c low.c medium.c file.c btree.c node.c \
			  record.c volume.c hfs.c version.c
OBJS		= os.o data.o block.o low.o medium.o file.o btree.o node.o \
			  record.o volume.o hfs.o version.o

STATIC_PRODUCT	= libhfs.a
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

