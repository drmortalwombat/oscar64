CC=../../bin/oscar64
CFLAGS=

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: helloworld.prg

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
