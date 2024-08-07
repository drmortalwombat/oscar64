CC=../../bin/oscar64
CFLAGS=

%.prg: %.c
	$(CC) $(CFLAGS) $<

all: largemem.prg allmem.prg charsetlo.prg charsethi.prg charsetcopy.prg charsetexpand.prg \
charsetload.prg easyflash.crt easyflashreloc.crt easyflashshared.crt tsr.prg overlay.prg

charsetload.prg: charsetload.c ../resources/charset.bin
	$(CC) $(CFLAGS) $< -d64=charsetload.d64 -fz=../resources/charset.bin

easyflash.crt: easyflash.c
	$(CC) $(CFLAGS) $< -n -tf=crt

easyflashreloc.crt: easyflashreloc.c
	$(CC) $(CFLAGS) $< -n -tf=crt

easyflashshared.crt: easyflashshared.c
	$(CC) $(CFLAGS) $< -n -tf=crt

tsr.prg: tsr.c
	$(CC) $(CFLAGS) $< -n -dNOFLOAT -dNOLONG

overlay.prg: overlay.c
	$(CC) $(CFLAGS) $< -n -d64=overlay.d64

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs *.d64 *.crt
