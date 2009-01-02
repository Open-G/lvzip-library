# Makefile for lvzlib
# Copyright (C) 1995-2003 Jean-loup Gailly.
# Copyright (C) 2003-2007 Rolf Kalbermatter.
# For conditions of distribution and use, see copyright notice in zlib.h

# To compile and test, type:
#   ./configure; make test
# The call of configure is optional if you don't have special requirements
# If you wish to build zlib as a shared library, use: ./configure -s

# To use the asm code, type:
#    cp contrib/asm?86/match.S ./match.S
#    make LOC=-DASMV OBJA=match.o

# To install /usr/local/lib/libz.* and /usr/local/include/zlib.h, type:
#    make install
# To install in $HOME instead of /usr/local, use:
#    make install prefix=$HOME

CC=cc

CFLAGS=-O
#CFLAGS=-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS=-g -DDEBUG
#CFLAGS=-O3 -Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#           -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS=lvzlib.a
LDSHARED=$(CC) -shared -Wl
CPP=$(CC) -E

LIBS=lvzlib.so
SHAREDLIB=lvzlib.so

AR=ar rc
RANLIB=ranlib
TAR=tar
SHELL=/bin/sh
EXE=

prefix = /usr/local
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
includedir = ${prefix}/include
mandir = ${prefix}/share/man
man3dir = ${mandir}/man3

OBJS = adler32.o compress.o crc32.o uncompr.o deflate.o trees.o \
       zutil.o inflate.o infback.o inftrees.o inffast.o \
       ioapi.o iomem.o lvutil.o zip.o unzip.o

OBJA =
# to use the asm code: make OBJA=match.o

TEST_OBJS = example.o minigzip.o

all: $(SHAREDLIB)

lvzlib.a: $(OBJS) $(OBJA)
	$(AR) $@ $(OBJS) $(OBJA)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

match.o: match.S
	$(CPP) match.S > _match.s
	$(CC) -c _match.s
	mv _match.o match.o
	rm -f _match.s

$(SHAREDLIB): $(OBJS) $(OBJA)
	$(LDSHARED) -o $@ $(OBJS) $(OBJA)

install: $(LIBS)
	-@if [ ! -d $(exec_prefix) ]; then mkdir -p $(exec_prefix); fi
	-@if [ ! -d $(includedir)  ]; then mkdir -p $(includedir); fi
	-@if [ ! -d $(libdir)      ]; then mkdir -p $(libdir); fi
	-@if [ ! -d $(man3dir)     ]; then mkdir -p $(man3dir); fi
	cp zlib.h zconf.h $(includedir)
	chmod 644 $(includedir)/zlib.h $(includedir)/zconf.h
	cp $(LIBS) $(libdir)
	cd $(libdir); chmod 755 $(LIBS)
	-@(cd $(libdir); $(RANLIB) libz.a || true) >/dev/null 2>&1
	cd $(libdir); if test -f $(SHAREDLIBV); then \
	  rm -f $(SHAREDLIB) $(SHAREDLIBM); \
	  ln -s $(SHAREDLIBV) $(SHAREDLIB); \
	  ln -s $(SHAREDLIBV) $(SHAREDLIBM); \
	  (ldconfig || true)  >/dev/null 2>&1; \
	fi
	cp zlib.3 $(man3dir)
	chmod 644 $(man3dir)/zlib.3
# The ranlib in install is needed on NeXTSTEP which checks file times
# ldconfig is for Linux

uninstall:
	cd $(includedir); \
	cd $(libdir); rm -f libz.a; \
	if test -f $(SHAREDLIBV); then \
	  rm -f $(SHAREDLIBV) $(SHAREDLIB) $(SHAREDLIBM); \
	fi
	cd $(man3dir); rm -f zlib.3

mostlyclean: clean
clean:
	rm -f *.o *~ example$(EXE) minigzip$(EXE) \
	   libz.* foo.gz so_locations \
	   _match.s maketree contrib/infback9/*.o

maintainer-clean: distclean
distclean: clean
	cp -p Makefile.in Makefile
	cp -p zconf.in.h zconf.h
	rm -f .DS_Store

tags:
	etags *.[ch]

depend:
	makedepend -- $(CFLAGS) -- *.[ch]

# DO NOT DELETE THIS LINE -- make depend depends on it.

adler32.o: zlib.h zconf.h
compress.o: zlib.h zconf.h
crc32.o: zutil.h zlib.h zconf.h /usr/include/string.h /usr/include/features.h
crc32.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
crc32.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
crc32.o: /usr/include/stdlib.h /usr/include/sys/types.h
crc32.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
crc32.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
crc32.o: /usr/include/sys/select.h /usr/include/bits/select.h
crc32.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
crc32.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
crc32.o: /usr/include/alloca.h /usr/include/errno.h /usr/include/bits/errno.h
crc32.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
crc32.o: /usr/include/asm-i386/errno.h /usr/include/asm-generic/errno.h
crc32.o: /usr/include/asm-generic/errno-base.h /usr/include/limits.h
crc32.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
crc32.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h crc32.h
deflate.o: deflate.h zutil.h zlib.h zconf.h /usr/include/string.h
deflate.o: /usr/include/features.h /usr/include/sys/cdefs.h
deflate.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
deflate.o: /usr/include/gnu/stubs-32.h /usr/include/stdlib.h
deflate.o: /usr/include/sys/types.h /usr/include/bits/types.h
deflate.o: /usr/include/bits/typesizes.h /usr/include/time.h
deflate.o: /usr/include/endian.h /usr/include/bits/endian.h
deflate.o: /usr/include/sys/select.h /usr/include/bits/select.h
deflate.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
deflate.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
deflate.o: /usr/include/alloca.h /usr/include/errno.h
deflate.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
deflate.o: /usr/include/asm/errno.h /usr/include/asm-i386/errno.h
deflate.o: /usr/include/asm-generic/errno.h
deflate.o: /usr/include/asm-generic/errno-base.h
deflate.o: zutil.h zlib.h zconf.h /usr/include/string.h
deflate.o: /usr/include/features.h /usr/include/sys/cdefs.h
deflate.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
deflate.o: /usr/include/gnu/stubs-32.h /usr/include/stdlib.h
deflate.o: /usr/include/sys/types.h /usr/include/bits/types.h
deflate.o: /usr/include/bits/typesizes.h /usr/include/time.h
deflate.o: /usr/include/endian.h /usr/include/bits/endian.h
deflate.o: /usr/include/sys/select.h /usr/include/bits/select.h
deflate.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
deflate.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
deflate.o: /usr/include/alloca.h /usr/include/errno.h
deflate.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
deflate.o: /usr/include/asm/errno.h /usr/include/asm-i386/errno.h
deflate.o: /usr/include/asm-generic/errno.h
deflate.o: /usr/include/asm-generic/errno-base.h
infback.o: zutil.h zlib.h zconf.h /usr/include/string.h
infback.o: /usr/include/features.h /usr/include/sys/cdefs.h
infback.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
infback.o: /usr/include/gnu/stubs-32.h /usr/include/stdlib.h
infback.o: /usr/include/sys/types.h /usr/include/bits/types.h
infback.o: /usr/include/bits/typesizes.h /usr/include/time.h
infback.o: /usr/include/endian.h /usr/include/bits/endian.h
infback.o: /usr/include/sys/select.h /usr/include/bits/select.h
infback.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
infback.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
infback.o: /usr/include/alloca.h /usr/include/errno.h
infback.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
infback.o: /usr/include/asm/errno.h /usr/include/asm-i386/errno.h
infback.o: /usr/include/asm-generic/errno.h
infback.o: /usr/include/asm-generic/errno-base.h inftrees.h inflate.h
infback.o: inffast.h inffixed.h
inffast.o: zutil.h zlib.h zconf.h /usr/include/string.h
inffast.o: /usr/include/features.h /usr/include/sys/cdefs.h
inffast.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
inffast.o: /usr/include/gnu/stubs-32.h /usr/include/stdlib.h
inffast.o: /usr/include/sys/types.h /usr/include/bits/types.h
inffast.o: /usr/include/bits/typesizes.h /usr/include/time.h
inffast.o: /usr/include/endian.h /usr/include/bits/endian.h
inffast.o: /usr/include/sys/select.h /usr/include/bits/select.h
inffast.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
inffast.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
inffast.o: /usr/include/alloca.h /usr/include/errno.h
inffast.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
inffast.o: /usr/include/asm/errno.h /usr/include/asm-i386/errno.h
inffast.o: /usr/include/asm-generic/errno.h
inffast.o: /usr/include/asm-generic/errno-base.h inftrees.h inflate.h
inffast.o: inffast.h
inflate.o: zutil.h zlib.h zconf.h /usr/include/string.h
inflate.o: /usr/include/features.h /usr/include/sys/cdefs.h
inflate.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
inflate.o: /usr/include/gnu/stubs-32.h /usr/include/stdlib.h
inflate.o: /usr/include/sys/types.h /usr/include/bits/types.h
inflate.o: /usr/include/bits/typesizes.h /usr/include/time.h
inflate.o: /usr/include/endian.h /usr/include/bits/endian.h
inflate.o: /usr/include/sys/select.h /usr/include/bits/select.h
inflate.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
inflate.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
inflate.o: /usr/include/alloca.h /usr/include/errno.h
inflate.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
inflate.o: /usr/include/asm/errno.h /usr/include/asm-i386/errno.h
inflate.o: /usr/include/asm-generic/errno.h
inflate.o: /usr/include/asm-generic/errno-base.h inftrees.h inflate.h
inflate.o: inffast.h inffixed.h
inftrees.o: zutil.h zlib.h zconf.h /usr/include/string.h
inftrees.o: /usr/include/features.h /usr/include/sys/cdefs.h
inftrees.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
inftrees.o: /usr/include/gnu/stubs-32.h /usr/include/stdlib.h
inftrees.o: /usr/include/sys/types.h /usr/include/bits/types.h
inftrees.o: /usr/include/bits/typesizes.h /usr/include/time.h
inftrees.o: /usr/include/endian.h /usr/include/bits/endian.h
inftrees.o: /usr/include/sys/select.h /usr/include/bits/select.h
inftrees.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
inftrees.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
inftrees.o: /usr/include/alloca.h /usr/include/errno.h
inftrees.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
inftrees.o: /usr/include/asm/errno.h /usr/include/asm-i386/errno.h
inftrees.o: /usr/include/asm-generic/errno.h
inftrees.o: /usr/include/asm-generic/errno-base.h inftrees.h
ioapi.o: /usr/include/stdio.h /usr/include/features.h
ioapi.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ioapi.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ioapi.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ioapi.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
ioapi.o: /usr/include/bits/wchar.h /usr/include/gconv.h
ioapi.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ioapi.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
ioapi.o: /usr/include/endian.h /usr/include/bits/endian.h
ioapi.o: /usr/include/sys/select.h /usr/include/bits/select.h
ioapi.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ioapi.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
ioapi.o: /usr/include/alloca.h /usr/include/string.h zlib.h zconf.h ioapi.h
iomem.o: zutil.h zlib.h zconf.h /usr/include/string.h /usr/include/features.h
iomem.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
iomem.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
iomem.o: /usr/include/stdlib.h /usr/include/sys/types.h
iomem.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
iomem.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
iomem.o: /usr/include/sys/select.h /usr/include/bits/select.h
iomem.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
iomem.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
iomem.o: /usr/include/alloca.h /usr/include/errno.h /usr/include/bits/errno.h
iomem.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
iomem.o: /usr/include/asm-i386/errno.h /usr/include/asm-generic/errno.h
iomem.o: /usr/include/asm-generic/errno-base.h ioapi.h lvutil.h iomem.h
iomem.o: /usr/include/stdlib.h /usr/include/features.h
iomem.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
iomem.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
iomem.o: /usr/include/sys/types.h /usr/include/bits/types.h
iomem.o: /usr/include/bits/typesizes.h /usr/include/time.h
iomem.o: /usr/include/endian.h /usr/include/bits/endian.h
iomem.o: /usr/include/sys/select.h /usr/include/bits/select.h
iomem.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
iomem.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
iomem.o: /usr/include/alloca.h
lvutil.o: /usr/include/stdio.h /usr/include/features.h
lvutil.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
lvutil.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
lvutil.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
lvutil.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
lvutil.o: /usr/include/bits/wchar.h /usr/include/gconv.h
lvutil.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
lvutil.o: /usr/include/string.h zlib.h zconf.h ioapi.h lvutil.h iomem.h
lvutil.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
lvutil.o: /usr/include/endian.h /usr/include/bits/endian.h
lvutil.o: /usr/include/sys/select.h /usr/include/bits/select.h
lvutil.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
lvutil.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
lvutil.o: /usr/include/alloca.h /usr/include/sys/stat.h
lvutil.o: /usr/include/bits/stat.h /usr/include/utime.h
mztools.o: /usr/include/stdio.h /usr/include/features.h
mztools.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
mztools.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
mztools.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
mztools.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
mztools.o: /usr/include/bits/wchar.h /usr/include/gconv.h
mztools.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
mztools.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
mztools.o: /usr/include/endian.h /usr/include/bits/endian.h
mztools.o: /usr/include/sys/select.h /usr/include/bits/select.h
mztools.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
mztools.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
mztools.o: /usr/include/alloca.h /usr/include/string.h zlib.h zconf.h unzip.h
mztools.o: ioapi.h
mztools.o: zlib.h zconf.h unzip.h ioapi.h
trees.o: deflate.h zutil.h zlib.h zconf.h /usr/include/string.h
trees.o: /usr/include/features.h /usr/include/sys/cdefs.h
trees.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
trees.o: /usr/include/gnu/stubs-32.h /usr/include/stdlib.h
trees.o: /usr/include/sys/types.h /usr/include/bits/types.h
trees.o: /usr/include/bits/typesizes.h /usr/include/time.h
trees.o: /usr/include/endian.h /usr/include/bits/endian.h
trees.o: /usr/include/sys/select.h /usr/include/bits/select.h
trees.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
trees.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
trees.o: /usr/include/alloca.h /usr/include/errno.h /usr/include/bits/errno.h
trees.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
trees.o: /usr/include/asm-i386/errno.h /usr/include/asm-generic/errno.h
trees.o: /usr/include/asm-generic/errno-base.h trees.h
uncompr.o: zlib.h zconf.h
unzip.o: /usr/include/stdio.h /usr/include/features.h
unzip.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
unzip.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
unzip.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
unzip.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
unzip.o: /usr/include/bits/wchar.h /usr/include/gconv.h
unzip.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
unzip.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
unzip.o: /usr/include/endian.h /usr/include/bits/endian.h
unzip.o: /usr/include/sys/select.h /usr/include/bits/select.h
unzip.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
unzip.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
unzip.o: /usr/include/alloca.h /usr/include/string.h zlib.h zconf.h unzip.h
unzip.o: ioapi.h /usr/include/errno.h /usr/include/bits/errno.h
unzip.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
unzip.o: /usr/include/asm-i386/errno.h /usr/include/asm-generic/errno.h
unzip.o: /usr/include/asm-generic/errno-base.h crypt.h
unzip.o: zlib.h zconf.h ioapi.h
zip.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
zip.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
zip.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
zip.o: /usr/include/bits/typesizes.h /usr/include/libio.h
zip.o: /usr/include/_G_config.h /usr/include/wchar.h
zip.o: /usr/include/bits/wchar.h /usr/include/gconv.h
zip.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
zip.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
zip.o: /usr/include/endian.h /usr/include/bits/endian.h
zip.o: /usr/include/sys/select.h /usr/include/bits/select.h
zip.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
zip.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
zip.o: /usr/include/alloca.h /usr/include/string.h zlib.h zconf.h zip.h
zip.o: ioapi.h /usr/include/errno.h /usr/include/bits/errno.h
zip.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
zip.o: /usr/include/asm-i386/errno.h /usr/include/asm-generic/errno.h
zip.o: /usr/include/asm-generic/errno-base.h crypt.h
zip.o: zlib.h zconf.h ioapi.h
zlib.o: zconf.h
zutil.o: zutil.h zlib.h zconf.h /usr/include/string.h /usr/include/features.h
zutil.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
zutil.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
zutil.o: /usr/include/stdlib.h /usr/include/sys/types.h
zutil.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
zutil.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
zutil.o: /usr/include/sys/select.h /usr/include/bits/select.h
zutil.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
zutil.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
zutil.o: /usr/include/alloca.h /usr/include/errno.h /usr/include/bits/errno.h
zutil.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
zutil.o: /usr/include/asm-i386/errno.h /usr/include/asm-generic/errno.h
zutil.o: /usr/include/asm-generic/errno-base.h
zutil.o: zlib.h zconf.h /usr/include/string.h /usr/include/features.h
zutil.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
zutil.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
zutil.o: /usr/include/stdlib.h /usr/include/sys/types.h
zutil.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
zutil.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
zutil.o: /usr/include/sys/select.h /usr/include/bits/select.h
zutil.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
zutil.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
zutil.o: /usr/include/alloca.h /usr/include/errno.h /usr/include/bits/errno.h
zutil.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
zutil.o: /usr/include/asm-i386/errno.h /usr/include/asm-generic/errno.h
zutil.o: /usr/include/asm-generic/errno-base.h
