CC=../../bin/oscar64
CFLAGS=-n

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: splitscreen.prg func3d.prg lines.prg polygon.prg bitblit.prg fractaltree.prg qsort.prg

splitscreen.prg: splitscreen.c
	$(CC) $<

clean:
	$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
