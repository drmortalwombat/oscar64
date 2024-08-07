%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: diskdir.prg filewrite.prg fileread.prg charwrite.prg charread.prg hireswrite.prg hiresread.prg

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs
