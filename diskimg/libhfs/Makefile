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

os.o: config.h /usr/include/fcntl.h /usr/include/features.h
os.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
os.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
os.o: /usr/include/bits/fcntl.h /usr/include/sys/types.h
os.o: /usr/include/bits/types.h
os.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
os.o: /usr/include/bits/typesizes.h /usr/include/time.h /usr/include/unistd.h
os.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
os.o: /usr/include/errno.h /usr/include/bits/errno.h
os.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
os.o: /usr/include/sys/stat.h /usr/include/bits/stat.h /usr/include/stdlib.h
os.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
os.o: /usr/include/wchar.h /usr/include/bits/wchar.h /usr/include/gconv.h
os.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stdarg.h
os.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h libhfs.h
os.o: hfs.h apple.h os.h
data.o: config.h /usr/include/string.h /usr/include/features.h
data.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
data.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
data.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
data.o: /usr/include/time.h /usr/include/bits/types.h
data.o: /usr/include/bits/typesizes.h data.h
block.o: config.h /usr/include/stdlib.h /usr/include/features.h
block.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
block.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
block.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
block.o: /usr/include/string.h /usr/include/errno.h /usr/include/bits/errno.h
block.o: /usr/include/linux/errno.h /usr/include/asm/errno.h libhfs.h hfs.h
block.o: /usr/include/time.h /usr/include/bits/types.h
block.o: /usr/include/bits/typesizes.h apple.h volume.h block.h os.h
low.o: config.h /usr/include/stdlib.h /usr/include/features.h
low.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
low.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
low.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
low.o: /usr/include/string.h /usr/include/errno.h /usr/include/bits/errno.h
low.o: /usr/include/linux/errno.h /usr/include/asm/errno.h libhfs.h hfs.h
low.o: /usr/include/time.h /usr/include/bits/types.h
low.o: /usr/include/bits/typesizes.h apple.h low.h data.h block.h file.h
medium.o: config.h /usr/include/stdlib.h /usr/include/features.h
medium.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
medium.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
medium.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
medium.o: /usr/include/string.h /usr/include/errno.h
medium.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
medium.o: /usr/include/asm/errno.h libhfs.h hfs.h /usr/include/time.h
medium.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h apple.h
medium.o: block.h low.h medium.h
file.o: config.h /usr/include/string.h /usr/include/features.h
file.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
file.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
file.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
file.o: /usr/include/errno.h /usr/include/bits/errno.h
file.o: /usr/include/linux/errno.h /usr/include/asm/errno.h libhfs.h hfs.h
file.o: /usr/include/time.h /usr/include/bits/types.h
file.o: /usr/include/bits/typesizes.h apple.h file.h btree.h record.h
file.o: volume.h
btree.o: config.h /usr/include/stdlib.h /usr/include/features.h
btree.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
btree.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
btree.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
btree.o: /usr/include/string.h /usr/include/errno.h /usr/include/bits/errno.h
btree.o: /usr/include/linux/errno.h /usr/include/asm/errno.h libhfs.h hfs.h
btree.o: /usr/include/time.h /usr/include/bits/types.h
btree.o: /usr/include/bits/typesizes.h apple.h btree.h data.h file.h block.h
btree.o: node.h
node.o: config.h /usr/include/stdlib.h /usr/include/features.h
node.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
node.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
node.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
node.o: /usr/include/string.h /usr/include/errno.h /usr/include/bits/errno.h
node.o: /usr/include/linux/errno.h /usr/include/asm/errno.h libhfs.h hfs.h
node.o: /usr/include/time.h /usr/include/bits/types.h
node.o: /usr/include/bits/typesizes.h apple.h node.h data.h btree.h
record.o: config.h /usr/include/string.h /usr/include/features.h
record.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
record.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
record.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h libhfs.h
record.o: hfs.h /usr/include/time.h /usr/include/bits/types.h
record.o: /usr/include/bits/typesizes.h apple.h /usr/include/errno.h
record.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
record.o: /usr/include/asm/errno.h record.h data.h
volume.o: config.h /usr/include/stdlib.h /usr/include/features.h
volume.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
volume.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
volume.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
volume.o: /usr/include/string.h /usr/include/time.h /usr/include/bits/types.h
volume.o: /usr/include/bits/typesizes.h /usr/include/errno.h
volume.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
volume.o: /usr/include/asm/errno.h /usr/include/assert.h /usr/include/stdio.h
volume.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
volume.o: /usr/include/bits/wchar.h /usr/include/gconv.h
volume.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stdarg.h
volume.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
volume.o: libhfs.h hfs.h apple.h volume.h data.h block.h low.h medium.h
volume.o: file.h btree.h record.h os.h
hfs.o: config.h /usr/include/stdlib.h /usr/include/features.h
hfs.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
hfs.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs-32.h
hfs.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stddef.h
hfs.o: /usr/include/string.h /usr/include/time.h /usr/include/bits/types.h
hfs.o: /usr/include/bits/typesizes.h /usr/include/errno.h
hfs.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
hfs.o: /usr/include/asm/errno.h /usr/include/assert.h /usr/include/stdio.h
hfs.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
hfs.o: /usr/include/bits/wchar.h /usr/include/gconv.h
hfs.o: /usr/lib/gcc/i386-redhat-linux/4.0.0/include/stdarg.h
hfs.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h libhfs.h
hfs.o: hfs.h apple.h data.h block.h medium.h file.h btree.h node.h record.h
hfs.o: volume.h
version.o: version.h
