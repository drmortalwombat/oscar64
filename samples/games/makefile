%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: snake.prg lander.prg maze3d.prg missile.prg breakout.prg connectfour.prg hscrollshmup.prg

snake.prg: snake.c
	@$(OSCAR64_CC) $<

missile.prg: missile.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) -O3 $<

hscrollshmup.prg: hscrollshmup.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) -O2 $<

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
