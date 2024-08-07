%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: func3d.prg polygon.prg floodfill.prg paint.prg

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg
