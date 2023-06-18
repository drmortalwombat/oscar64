CC=../../bin/oscar64
CFLAGS=-n

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: snake.prg lander.prg maze3d.prg missile.prg breakout.prg connectfour.prg hscrollshmup.prg

snake.prg: snake.c
	$(CC) $<

missile.prg: missile.c
	$(CC) $(CFLAGS) -O3 $<

hscrollshmup.prg: hscrollshmup.c
	$(CC) $(CFLAGS) -O2 $<

clean:
	$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
