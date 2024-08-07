CC=../../bin/oscar64
CFLAGS=-n

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: bigfont.prg tunnel.prg grid2d.prg colorram.prg cgrid8way.prg

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg
