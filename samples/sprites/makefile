CC=../../bin/oscar64
CFLAGS=-n

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: joycontrol.prg multiplexer.prg creditroll.prg sprmux32.prg sprmux64.prg

joycontrol.prg: joycontrol.c
	$(CC) $<

sprmux32.prg: sprmux32.c
	$(CC) $(CFLAGS) $< -O2 -dVSPRITES_MAX=32 -dNUM_IRQS=28

clean:
	$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
