%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: colorbars.prg openborder.prg textcrawler.prg movingbars.prg

movingbars.prg: movingbars.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -n

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
