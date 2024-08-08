%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: fireworks_ptr.prg fireworks_hires.prg fireworks_stripe.prg

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg
