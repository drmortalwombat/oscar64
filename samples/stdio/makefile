%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: helloworld.prg

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
