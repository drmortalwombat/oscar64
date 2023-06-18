CC=../../bin/oscar64
CFLAGS=-n

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: func3d.prg polygon.prg floodfill.prg paint.prg

clean:
	$(RM) *.asm *.int *.lbl *.map *.prg
