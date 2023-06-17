CC=../../bin/oscar64
CFLAGS=

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: colorbars.prg openborder.prg textcrawler.prg movingbars.prg

movingbars.prg: movingbars.c
	$(CC) $(CFLAGS) $< -n

clean:
	$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
