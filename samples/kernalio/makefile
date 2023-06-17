CC=../../bin/oscar64
CFLAGS=

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: diskdir.prg filewrite.prg fileread.prg charwrite.prg charread.prg hireswrite.prg hiresread.prg

clean:
	$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
