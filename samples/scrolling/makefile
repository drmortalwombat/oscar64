%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: bigfont.prg tunnel.prg grid2d.prg colorram.prg cgrid8way.prg

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg
