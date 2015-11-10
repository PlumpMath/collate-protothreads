# Makefile edited to include collate.c

#CFLAGS=-O -Wuninitialized -Werror
CFLAGS=-g -Wuninitialized -Werror

all: example-codelock example-buffer example-small collate

example-codelock: example-codelock.c pt.h lc.h

example-buffer: example-buffer.c pt.h lc.h

example-small: example-small.c pt.h lc.h

collate: collate.c pt.h lc.h
