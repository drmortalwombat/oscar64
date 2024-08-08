%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: splitscreen.prg func3d.prg lines.prg polygon.prg bitblit.prg cube3d.prg fractaltree.prg qsort.prg

splitscreen.prg: splitscreen.c
	@$(OSCAR64_CC) $<

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
