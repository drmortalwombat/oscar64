%.prg: %.c
	@echo "Compiling sample file" $<
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $<

all: largemem.prg allmem.prg charsetlo.prg charsethi.prg charsetcopy.prg charsetexpand.prg \
charsetload.prg easyflash.crt easyflashreloc.crt easyflashshared.crt tsr.prg overlay.prg

charsetload.prg: charsetload.c ../resources/charset.bin
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -d64=charsetload.d64 -fz=../resources/charset.bin

easyflash.crt: easyflash.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -n -tf=crt

easyflashreloc.crt: easyflashreloc.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -n -tf=crt

easyflashshared.crt: easyflashshared.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -n -tf=crt

tsr.prg: tsr.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -n -dNOFLOAT -dNOLONG

overlay.prg: overlay.c
	@$(OSCAR64_CC) $(OSCAR64_CFLAGS) $< -n -d64=overlay.d64

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs *.d64 *.crt
