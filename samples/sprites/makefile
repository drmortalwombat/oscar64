%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: joycontrol.prg multiplexer.prg creditroll.prg sprmux32.prg sprmux64.prg

joycontrol.prg: joycontrol.c
	@$(OSCAR64_CC) $<

sprmux32.prg: sprmux32.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -O2 -dVSPRITES_MAX=32 -dNUM_IRQS=28

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
