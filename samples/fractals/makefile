CC=../../bin/oscar64
CFLAGS=-n

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: mbtext.prg mbtext.prg mbhires.prg mbmulti.prg mbmulti3d.prg mbfixed.prg mbzoom.prg

mbfixed.prg: mbfixed.c
	$(CC) $(CFLAGS) -O3 $<

mbzoom.prg: mbzoom.c
	$(CC) $(CFLAGS) -O3 $<

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg
