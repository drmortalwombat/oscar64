#!/bin/sh
../../bin/oscar64 largemem.c
../../bin/oscar64 allmem.c
../../bin/oscar64 charsetlo.c
../../bin/oscar64 charsethi.c
../../bin/oscar64 charsetcopy.c
../../bin/oscar64 easyflash.c -n -tf=crt
../../bin/oscar64 easyflashreloc.c -n -tf=crt
../../bin/oscar64 easyflashshared.c -n -tf=crt
../../bin/oscar64 tsr.c -n -dNOFLOAT -dNOLONG
