%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: mbtext.prg mbtext.prg mbhires.prg mbmulti.prg mbmulti3d.prg mbfixed.prg mbzoom.prg

mbfixed.prg: mbfixed.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) -O3 $<

mbzoom.prg: mbzoom.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) -O3 $<

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg
