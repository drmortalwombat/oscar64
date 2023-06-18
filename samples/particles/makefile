CC=../../bin/oscar64
CFLAGS=-n

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: fireworks_ptr.prg fireworks_hires.prg fireworks_stripe.prg

clean:
	$(RM) *.asm *.int *.lbl *.map *.prg
